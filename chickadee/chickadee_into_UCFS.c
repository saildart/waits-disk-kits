#if 0 // -*- mode:C;coding:utf-8; -*-
NAME="chickadee_into_UCFS"
  full_path=$(realpath $0)
  echo FROM $full_path
  SRC=$(dirname $full_path)
  DST="/usr/local/bin"
  gcc -g -m64 -Wall -Werror -o $DST/$NAME  $SRC/$NAME.c && echo -n OK || echo -n FAILED
  echo " $DST/$NAME"
  return 2>/dev/null || exit 0
#endif
  /*
    program: chickadee_into_UCFS
    nee:     unload_ckd
    associated: DASD_into_chickadee
    associated: ralf
    rationale:
  */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h> 
  // ToDo: better arrangement of source material pathnames
#include "utf8.h"
#include "utf8.c"
#include "/data/KA10sn32/library/kayten.c"
  //
#define perr(msg) if(1){perror(msg);exit(EXIT_FAILURE);}
#define bamboo // message clutter management markers
#define bamoff if(0)
#define bamon  if(1)
  //
typedef unsigned char      uint8; // bit width type names
typedef unsigned short int uint16;
typedef unsigned       int uint32;

/* ================================================================================
           G       L       O       B       A       L
   ================================================================================
   Warnings:
   ⇥    There is too much global state in this program.
   ⇥    Funky use of uppercase names SAT and UFD.
   ⇥    SAIL-WAITS jargon does not distinguish UFD-slots, UFD-files and PPNs.
   ⇥    The SAIL-WAITS file system exploited directory and file equivalence.
   ⇥    The empty input 0.ckd will generate a 1 file, 1 UFD, 1 ppn UCFS model.
   .    so called "empty" UCFS/1.1/__1__1.UFD file system with its one directory.
   Notes:
   ⇥    Every SAIL-WAITS file had a UFD-slot inside a UFD-directory-file.
   ⇥    UFD-files were named *.UFD[1,1] and contained directories of UFD-slots.
   ⇥    The root directory was named "  1  1.UFD[1,1]" and had one-slot for each PPN.
   ⇥    PPN is a Project-Programmer-Name for example [1,BGB] as well as [1,1]
*/
RIB_t rib;      // Thirty-two PDP10 words, followed by 2304. PDP10 words. (18*128 is 2304)
UFD_t ufd[576]; // four PDP10 words per UFD dir entry. 2304./4 = 576. per track.
UFD_t *UFDslot; // Allocated by rib_handler. Filled in by the payload_handler.
//
int ufdpos=1; // next UFD-slot for the file-track-follower.
int ufdcnt=0; // total number of files seen over all [*,*] directories
int ppncnt=0; // total number of UFD files in the [1,1] root directory
//
uint64 record[18*128]; // 2304. PDP10 words per track
uint64 SAT[1267];      // WAITS sacred Storage-Allocation-Table bit map of used tracks.
//
SATHEAD_t sathead;
SAT_t     satbody;
int flag_into_tracks=0;
int flag_into_UCFS=0;
struct stat statbuf;
FILE *disk[10], *csv, *logr;
/*
  DATA8 has one PDP-10 word placed in 64-bits right adjusted.
  DATA9 has two PDP-10 words packed into 9 bytes.
  --
  Cornwell places the two PDP10-word bit-patterns(0xDeadBeef5 and 0xDeadBeef6) into 9 bytes as 0xDeadBeef56DeadBeef.
  Baumgart places the two PDP10-word bit-patterns(0xDeadBeef5 and 0xDeadBeef6) into 9 bytes as 0xDeadBeef5DeadBeef6.
  --
  This program follows the Cornwell convention to decode CKD for the Cornwell PDP10 simulation.
*/


void
touch_file_modtime( char *path, char *iso_dt ){
  struct tm tm;
  struct utimbuf utm;
  //    
  bzero( &tm, sizeof(tm));
  strptime( iso_dt, "%Y-%m-%dT%H:%M:%S", &tm );
  utm.actime = utm.modtime = mktime( &tm );
  utime( path, &utm );
}
// ToDo: document the data9 bit/byte formats; write a manifesto/polemic about my preference.
#if 0
// Baumgart, two PDP10 words deadbeef5 deadbeef6 <<==>> CKD deadbeef5deadbeef6 bit pattern.
typedef union {
  // Little endian x86 struct
  struct { uint64  byte1:8, byte2:8, byte3:8, byte4:8, nibble_lo:4,:28; } even;
  struct { uint64 nibble_hi:4, byte6:8, byte7:8, byte8:8, byte9:8, :28; } odd;
} data8_word_t;
#else
// Cornwell, two PDP10 words deadbeef5 deadbeef6 <<==>> CKD deadbeef56deadbeef bit pattern.
typedef union {
  // Little endian x86 struct
  struct { uint64 nibble_hi:4, byte4:8, byte3:8, byte2:8, byte1:8, :28; } even;
  struct { uint64 nibble_lo:4, byte9:8, byte8:8, byte7:8, byte6:8, :28; } odd;
} data8_word_t;
#endif

void
data9_into_data8_cromwell(uint8 *data9, data8_word_t *data8, int wrdcnt ){
  int m;
  for( m=0; m<wrdcnt; m+=2, data9+=9 ){
    data8[m].even.byte1 =     data9[0] ;
    data8[m].even.byte2 =     data9[1] ;
    data8[m].even.byte3 =     data9[2] ;
    data8[m].even.byte4 =     data9[3] ;
    data8[m].even.nibble_hi = data9[4] >> 4;
    if(m+1 < wrdcnt){
      data8[m+1].odd.nibble_lo = data9[4] & 0xF;
      data8[m+1].odd.byte6 = data9[5] ;
      data8[m+1].odd.byte7 = data9[6] ;
      data8[m+1].odd.byte8 = data9[7] ;
      data8[m+1].odd.byte9 = data9[8] ;
    }  
  }
}

// Model of Petit IBM track for CKD, Check-Key-Data, format.
struct pmp_header
{
  uint8    devid[8];      /* device header. */
  uint32   heads;         /* number of heads per cylinder */
  uint32   tracksize;     /* size of track */
  uint8    devtype;       /* Hex code of last two digits of device type. */
  uint8    fileseq;       /* always 0. */
  uint16   highcyl;       /* highest cylinder. */
  uint8    resv[492];     /* pad to 512 byte block */
};
uint8 start_record[13];   // The IBM key address cylinder head
uint8 rib_[144];          // SAIL-WAITS 32. PDP10 words in DATA9 format.
uint8 payload_[10512];
int   end_record;         // always -1 EOR marker
uint8 zero_padding[2783]; // always zero
// ===================================================================================================
//            R       E       A       D               T       R       A       C       K
// ===================================================================================================
void
read_track(int track){
  int unit, cyl, hd, cylinder=-1, head=-1, i, q=0;
  unit = ( track / 15200 ) ;
  cyl  = ( track % 15200 ) / 19;
  hd   = ( track % 19    ) ;
#define TRACKSIZE 13312
  // offset =  512 + (cyl*19 + hd) * TRACKSIZE;  fprintf(stderr,"fseek =%ld\n",offset);
  q = fseek( disk[unit], 512 + (cyl*19 + hd) * TRACKSIZE, SEEK_SET ); if(q<0) perr("fseek");
  // offset = ftell( disk[unit] );  fprintf(stderr,"ftell =%ld\n",offset);
  // Read one track record, 13312 bytes, as five fixed sized parts.
  if(1!=fread( &start_record,          13 ,   1,disk[unit])) perr("start record"); //     13. bytes
  if(1!=fread( &rib_,                16*9 ,   1,disk[unit])) perr("rib");          //    144. bytes
  if(1!=fread( &payload_,      18*(8+64*9),   1,disk[unit])) perr("payload");      // 10,512. bytes
  if(1!=fread( &end_record,             4 ,   1,disk[unit])) perr("end record");   //      4. bytes
  if(1!=fread( &zero_padding, 13312-(13+144+144+10368+4),                          //  2,639. bytes
               1,disk[unit])) perr("zero padding");
  // offset = ftell( disk[unit] );  fprintf(stderr,"ftell =%ld\n",offset);
  // Confirm track format IBM CKD compliance. CKD Check-Key-Data.
  cylinder = start_record[1]<<8 | start_record[2];
  head     = start_record[3]<<8 | start_record[4];
  if(1){
    assert( cyl == cylinder   );
    assert(  hd == head       );
    assert(  -1 == end_record ); // EOR mark confirmed.
  }
  // Reformat track RIB - Retrieval Information Block - 32 PDP10 words
  // from 9-bytes into 16-bytes for each two PDP10 words
  data9_into_data8_cromwell(    rib_,
                                (data8_word_t *)&rib,
                                32 );
  // Reformat track PAYLOAD - file content fragment - 2304 PDP10 words
  // from tracks having 18 IBM records,
  // each record with an 8 byte prefix to step over.
  for(i=0;i<18;i++){
    data9_into_data8_cromwell(  payload_ + 8 + i*(8+64*9),
                                (data8_word_t *)(&record[i*128]),
                                128 );
  }
  if(0)fprintf( stdout,  // IBM hardware address
                "read_track(%d) %d.pack %3d.cylinder %2d.head %6d.track"
                "  filnam %012lo ext %06o rib.track=%d rib.count=%d\n",
                track, unit, cylinder, head, track,
                (ulong)rib.filnam, rib.ext, (int)rib.track, (int)rib.count );
}

// ===================================================================================================
//      S       A       T       and     M       F       D               i n i t i a l i z a t i o n
// ===================================================================================================
void
SAT_and_MFD_initialization(){
  int i;
  UFD_t MFD;
  memcpy((void *) &record, (void *) &rib, 6*8 ); // move rib to record to see PDP10 words
  fprintf(stderr,
          "dskuse=%lld. lstblk=%lld. satid=%012llo "
          "satchk=%012llo badcnt=%lld. badchk=%llo\n",
          record[0],record[1],record[2],
          record[3],record[4],record[5]);
  sathead.dskuse = record[0];
  sathead.lstblk = record[1];
  sathead.satid  = record[2];
  sathead.satchk = record[3];
  sathead.badcnt = record[4];
  sathead.badchk = record[5];
  // SAT bit map into SAT array of uint64
  for(i=0;i<1267;i++){
    SAT[i] = record[(57-32)+i];
  }
  // Oversized allocation since file_count < used_track_count.
  UFDslot = (UFD_t *)calloc( sathead.dskuse, sizeof(UFD_t) );
  // Initialize the UFDslot[1] seed needed to read track #1 as a file fragment of "  1  1.UFD[1,1]".
  MFD.filnam = 000021000021L; // sixbit/  1  1/
  MFD.ext    = 0654644;       // sixbit/UFD/
  // My reënactment will try to implement Y2K (and beyond) datetime stamps.
  // This here root MFD belongs to or  Long-Now(circa 2021) and it shall be
  // good until the 2052-02-01 fifteen bit date overflow.
  {
    ulong now = waits_datetime_now();
    // Dear Time Lords !
    // --
    // We reach out and greet our friends on calendar date 47064-01-16,
    // which shall be the final 24-bit working day for this code.
    //
    // May I point out there are yet another 28-bits of left-word DATA8 padding to be used.
    // However my year 47K followers will have to expect the disappointment on Tuesday
    //                  12106450613175-01-04
    // when the 64 bit SAIL-WAITS date rolls over yet again.
    //
    // In the 20th century, on each roll over date, we would have an "end-of-world" party.
    // The year 2000, Y2K threat did NOT materialize.
    // I watched from California, as the New Year's day came upon my
    // New Zealand software clients; we passed that datetime mark without significant incident.
    // --
    // BGB circa 2020 (born 1946).    
    ulong d = now >>    12; // twenty-four bits of date
    ulong t = now & 07777L; // twelve bits of time
    //
    MFD.creation_date     = d;
    MFD.date_written_high = d >> 12;
    MFD.date_written_low  = d & 07777L;
    MFD.time_written      = t;
  }
  MFD.mode  = 017;      // binary TBX neo-mode-code
  MFD.prot  = 0;        // neo reënactment world is wide open
  MFD.track = 1;        // sacred SAIL-WAITS ralf root track one
  UFDslot[1]= MFD;      // Plop !
  // 000021000021 654644_created prot_mode_writ  track#1 // UFD for filename "  1  1.UFD[1,1]"
  //
  // 000021000021 654644000000 000000000000 000021000021 // RIB for filename "  1  1.UFD[1,1]"
  // 000000000001 000000000050 track_first   word_count
  // 000000007533 000000007533 ref_datetime dmp_datetime
  // 000000000001 000000000000   group       group_next
  //
  // 000000000001 xxxxxxxxxxxx yyyyyyyyyyyy zzzzzzzzzzzz      Track numbers of group 1
  // wwwwwwwwwwww xxxxxxxxxxxx yyyyyyyyyyyy zzzzzzzzzzzz
  // wwwwwwwwwwww xxxxxxxxxxxx yyyyyyyyyyyy zzzzzzzzzzzz
  // wwwwwwwwwwww xxxxxxxxxxxx yyyyyyyyyyyy zzzzzzzzzzzz
}

char *
sixbit_uint_into_ascii_(char *p0, uint64 x, char *tbl)
{
  pdp10_word_t w=(pdp10_word_t)x;
  char *p=p0;
  // SIXBIT into ASCII by arithmetic would by + 040
  *p++ = tbl[w.sixbit.c1];
  *p++ = tbl[w.sixbit.c2];
  *p++ = tbl[w.sixbit.c3];
  *p++ = tbl[w.sixbit.c4];
  *p++ = tbl[w.sixbit.c5];
  *p++ = tbl[w.sixbit.c6];
  *p++= 0;
  return p0;
}
char *
sixbit_halfword_into_ascii_(char *p0,int halfword,char *tbl)
{
  char *p=p0;
  *p++ = tbl[(halfword >> 12) & 077];
  *p++ = tbl[(halfword >>  6) & 077];
  *p++ = tbl[ halfword        & 077];
  *p++ = 0;
  return p0;
}
void
space_to_underbar(char *p)
{
  for (;*p;p++)
    if (*p==' ')
      *p = '_';
}
void
omit_spaces(char *q)
{
  char *t,*p;
  for (p= t= q; *p; p++)
    if (*p != ' ')
      *t++ = *p;
  *t++ = 0;
}

char *SAIL_ASCII[]=
  {
   // Decode SAIL seven bit codepoints into UTF8 strings.
   // Omit five codes 000 null, 013 vt, 015 cr, 0175 Stanford-ALT and 0177 rubout
   // 00      01      02      03          04      05      06      07
   "",       "↓",    "α",    "β",         "∧",    "¬",    "ε",    "π",  //  000
   "λ",     "\t",   "\n",     "",        "\f",   "",      "∞",    "∂",  //  010
   "⊂",      "⊃",    "∩",    "∪",         "∀",    "∃",    "⊗",    "↔",  //  020
   "_",      "→",    "~",    "≠",         "≤",    "≥",    "≡",    "∨",  //  030
   " ",      "!",   "\"",    "#",         "$",    "%",    "&",    "'",  //  040
   "(",      ")",    "*",    "+",         ",",    "-",    ".",    "/",  //  050
   "0",      "1",    "2",    "3",         "4",    "5",    "6",    "7",  //  060
   "8",      "9",    ":",    ";",         "<",    "=",    ">",    "?",  //  070
   "@",      "A",    "B",    "C",         "D",    "E",    "F",    "G",  // 0100
   "H",      "I",    "J",    "K",         "L",    "M",    "N",    "O",  // 0110
   "P",      "Q",    "R",    "S",         "T",    "U",    "V",    "W",  // 0120
   "X",      "Y",    "Z",    "[",         "\\",   "]",    "↑",    "←",  // 0130
   "'",      "a",    "b",    "c",         "d",    "e",    "f",    "g",  // 0140
   "h",      "i",    "j",    "k",         "l",    "m",    "n",    "o",  // 0150
   "p",      "q",    "r",    "s",         "t",    "u",    "v",    "w",  // 0160
   "x",      "y",    "z",    "{",         "|",    "",     "}",    ""    // 0170
  };

typedef union {
  uint64 fw;
  struct { uint64 word:36,:28;                          } full;
  struct { uint64 bit35:1,a5:7,a4:7,a3:7,a2:7,a1:7,:28;	} seven; // seven bit ASCII
} pdp10_word;

void
data8_into_utf8( char *data8path, char *utf8path ){
  // no frills conversion from the Stanford A.I. Lab ASCII into UTF-8 text.
  int i;  FILE *o; struct stat sblk;
  pdp10_word *buf8;
  off_t filesize; // fstat from linux file system is in bytes
  int m, word_count, byte_count;
  int omit_tab=0;
  i = open(data8path,O_RDONLY);
  if (i<0){ fprintf(stderr,"ERROR: input open file \"%s\" failed.\n",data8path); return; }
  if(fstat(i,&sblk)==-1) perr("fstat");
  filesize = sblk.st_size;
  buf8 = (pdp10_word *)malloc(filesize);
  byte_count = read(i,buf8,filesize);
  close(i);
  word_count = byte_count/8;
  o = fopen(utf8path,"w");
  if (o<0){ fprintf(stderr,"ERROR: output open file \"%s\" failed.\n",utf8path); return; }
  for(m=0;m<word_count;m++){
    if(buf8[m].seven.bit35){
      // omit SOS line number + TAB, as well as the SOS end-of-page "     \r\r\f" marker
      omit_tab = 1;
    } else {
      fprintf( o, "%s%s%s%s%s",
               omit_tab ? "" : // omit SOS tab
               SAIL_ASCII[ buf8[m].seven.a1 ],
               SAIL_ASCII[ buf8[m].seven.a2 ],
               SAIL_ASCII[ buf8[m].seven.a3 ],
               SAIL_ASCII[ buf8[m].seven.a4 ],
               SAIL_ASCII[ buf8[m].seven.a5 ] );
      omit_tab = 0;
    }
  }
  fclose(o);
  free( buf8 );
}

void
utf8_into_data8( char *utf8path, char *data8path ){  
  // No frills convert UTF-8 text into Stanford A.I. Lab ASCII
  // SAIL7 code as one UTF-8 string in 'C' notation, isolate backwacked characters.
  char *SAIL7=
    "␀↓αβ∧¬επλ"  "\t"   "\n"    "\v"    "\f"   "\r"
    "∞∂⊂⊃∩∪∀∃⊗↔_→~≠≤≥≡∨ !"        "\""
    "#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ["        "\\"    "]↑←"
    "`abcdefghijklmnopqrstuvwxyz{|§}"     "\b" ;
  
  int i; struct stat sblk; off_t filesize; // in bytes
  int o;

  char          *buf7; // From "7-bit SAIL" represented as UTF-8
  pdp10_word_t  *buf8; // Into "7-bit SAIL" represented as data8  

  int qcnt=0, k, m, u8_count, word_count;
  uint32 wchar;
  int sailcode=0, charn=0;
  
  i = open(utf8path,O_RDONLY);
  if (i<0){ fprintf(stderr,"ERROR: input open file \"%s\" failed.\n",utf8path); return; }
  if(fstat(i,&sblk)==-1) perr("fstat");
  filesize = sblk.st_size;
  buf7 = (char *)malloc(filesize);
  read(i,buf7,filesize);
  close(i);

  u8_count = u8_strlen( buf7 );
  buf8 = (pdp10_word_t *)malloc( 2*u8_count/5 + 1 );
  if(verbose)fprintf(stderr,"read %ld bytes, %d u8_characters\n", filesize, u8_count );

  o = open(data8path,O_RDWR|O_CREAT|O_TRUNC,0666);
  if (o<0){ fprintf(stderr,"ERROR: output open file \"%s\" failed.\n",data8path); return; }

  // convert UTF8 into DATA8 for SAIL WAITS
  for(k=1;k<=u8_count;k++){
        
    wchar = u8_nextchar( buf7, &charn  );
    u8_strchr( SAIL7, wchar, &sailcode );
    if(verbose)fprintf(stderr,"wchar %d sailcode %d\n",wchar,sailcode);
    assert( sailcode < 128 );
        
    // place one CR before each LF.
    if( sailcode==012 ){
      switch(qcnt%5){
      case 0: buf8[m].seven.a1=015; break;
      case 1: buf8[m].seven.a2=015; break;
      case 2: buf8[m].seven.a3=015; break;
      case 3: buf8[m].seven.a4=015; break;
      case 4: buf8[m].seven.a5=015; break;
      }
      qcnt++; // count output SAIL codes ( 7-bit "bytes" )
      m = qcnt/5;
    }
    // place one SAIL code 7-bit-character "byte" into DATA8 word
    switch( qcnt%5 ){
    case 0: buf8[m].seven.a1=sailcode; break;
    case 1: buf8[m].seven.a2=sailcode; break;
    case 2: buf8[m].seven.a3=sailcode; break;
    case 3: buf8[m].seven.a4=sailcode; break;
    case 4: buf8[m].seven.a5=sailcode; break;
    }
    qcnt++; // count output SAIL codes ( 7-bit "bytes" )
    m = qcnt/5;
  }
  word_count = m+1;
  write(o,buf8,word_count*8); // Write data8 binary file
  close(o);
  free( buf7 );
  free( buf8 );
}

void
data8_into_dasm( char *data8path, char *outpath ){
  // no frills conversion from the Stanford A.I. Lab ASCII into DASM diassembly listing
  int i;  FILE *o; struct stat sblk;
  pdp10_word_t *buf8;
  off_t filesize; // fstat from linux file system is in bytes
  int m, word_count, byte_count;

  i = open(data8path,O_RDONLY);
  if (i<0){ fprintf(stderr,"ERROR: input open file \"%s\" failed.\n",data8path); return; }
  if(fstat(i,&sblk)==-1) perr("fstat");
  filesize = sblk.st_size;
  buf8 = (pdp10_word_t *)malloc(filesize);
  byte_count = read(i,buf8,filesize);
  close(i);
  word_count = byte_count/8;
  o = fopen(outpath,"w");
  if (o<0){ fprintf(stderr,"ERROR: output open file \"%s\" failed.\n",outpath); return; }
  for(m=0;m<word_count;m++){
    ;
  }
  fclose(o);
  free( buf8 );
}

void  
data8_into_octal( char *data8path, char *outpath ){
  // no frills conversion from the Stanford A.I. Lab ASCII into UTF-8 text.
  int i;  FILE *o; struct stat sblk;
  pdp10_word_t *buf8;
  off_t filesize; // fstat from linux file system is in bytes
  int m, word_count, byte_count;

  i = open(data8path,O_RDONLY);
  if (i<0){ fprintf(stderr,"ERROR: input open file \"%s\" failed.\n",data8path); return; }
  if(fstat(i,&sblk)==-1) perr("fstat");
  filesize = sblk.st_size;
  buf8 = (pdp10_word_t *)malloc(filesize);
  byte_count = read(i,buf8,filesize);
  close(i);
  word_count = byte_count/8;
  o = fopen(outpath,"w");
  if (o<0){ fprintf(stderr,"ERROR: output open file \"%s\" failed.\n",outpath); return; }
  for(m=0;m<word_count;m++){
    fprintf( o, "%06o %06o\n", buf8[m].half.left, buf8[m].half.right);
  }
  fclose(o);
  free( buf8 );
}

// ===================================================================================================
//              R       I       B               H A N D L E R
// ===================================================================================================
int
rib_handler(int track){
  int i, track_offset=-1, next_track=0;
  int group, group_next;
  fprintf(stderr,"rib_handler(%d) rib.count=%d words\n", track, rib.count );
  if (track==0 || rib.count==0) return 0; // nothing to do here
  group =       rib.DGRP1R / 576; // 576=18*32
  group_next =  rib.DNXTGP;
  track_offset = -1;
#ifndef LRZ
#define LRZ(x)  (((x) >> 18) &  0777777)
#define RRZ(x)  ( (x)        &  0777777)
#endif
  for(i=0; i<16 && rib.dptr[i]!=0LL; i++){
    if (track == (int)LRZ(rib.dptr[i])){
      track_offset = group*32 + 2*i;
      next_track = (int)RRZ(rib.dptr[i]);
    }
    if(track == (int)RRZ(rib.dptr[i])){
      track_offset = group*32 + 2*i + 1;
      next_track = (i<15) ? (int)LRZ(rib.dptr[i+1]) : -1;
      // byte_offset = track_offset * 8 * 2304;
      // byte_count = wrdcnt * 8;
    }
  }
  // logger
  fprintf( stdout,
           "track x%06d  next_track x%06d  group=%d  group_next=%d   track_offset=%d\n",
           track,        next_track,       group,    group_next,     track_offset );
  for(i=0; i<16; i++){
    if(i%4==0)fprintf(stderr,"%2d/       ",2*i);else fprintf(stderr,"       ");
    fprintf(stderr, " %6lld %6lld    ",
            (uint64)LRZ(rib.dptr[i]),
            (uint64)RRZ(rib.dptr[i]));
    if(i%4==3)fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");
  return next_track;
}

/* P A S S ex           Linear Scan Exercise
   --------------------------------------------------------------------------- 80 ...... 90 ..... 100
   Linear scan of the Cornwell 0.ckd to 9.ckd disk packs input from KIT directory.
   Scan up to 10 packs, 800 cylinders, 19 heads; net 15200 tracks per pack.
   Each track header is a 32 PDP10 word RIB, 

   Retrieval Information Block,

   followed by up to 2304.
   PDP10 words of SAIL-WAIT file content.

   Payload file fragment

   The Cornwell SIMS IBM disk format has () additional bytes for address markers and zero padding.

   And there is an additional 512. byte IBM hardare pack label on the front of the CKD file.
*/
void
pass_ex(){
  struct pmp_header hdr;
  int unit, cyl,cylinder=-1, hd,head=-1, track;
  //
  for(unit=0;unit<=9;unit++){
    fseek( disk[unit], 0, SEEK_SET );
    fread( &hdr, sizeof(struct pmp_header), 1, disk[unit] );    
    fprintf(stdout, // IBM hardware pack label
              "devid '%8.8s'  == 'CKD_P370' ?\n"
              "heads     %6d  ==    19. ?\n"
              "tracksize %6d  == 13312. ?\n"
              "dev type    0x%0x  ==  0x30  ?\n"
              "highcyl   %6d  ==   815. ?\n",
              hdr.devid, hdr.heads, hdr.tracksize, hdr.devtype, hdr.highcyl );    
    for (cyl=0;cyl<800;cyl++){  // 800. cylinders
      for (hd=0;hd<19;hd++){    //  19. heads
        // Read one track record, 13312 bytes, as five fixed sized parts.
        if(1!=fread( &start_record,          13 ,   1,disk[unit])) perr("start record"); //     13. bytes
        if(1!=fread( &rib_,                16*9 ,   1,disk[unit])) perr("rib");          //    144. bytes
        if(1!=fread( &payload_,      18*(8+64*9),   1,disk[unit])) perr("payload");      // 10,512. bytes
        if(1!=fread( &end_record,             4 ,   1,disk[unit])) perr("end record");   //      4. bytes
        if(1!=fread( &zero_padding, 13312-(13+144+144+10368+4),                          //  2,639. bytes
                     1,disk[unit])) perr("zero padding");
        // Confirm track format IBM CKD compliance. CKD Check-Key-Data.
        cylinder = start_record[1]<<8 | start_record[2];
        head     = start_record[3]<<8 | start_record[4];
        assert( cyl == cylinder   );
        assert(  hd == head       );
        assert(  -1 == end_record ); // EOR mark confirmed.
        fprintf(stdout,"%d.pack %3d.cylinder %2d.head %6d.track\n", unit, cylinder, head, track); // IBM hardware address        
        track = ( unit*800*19 + cylinder*19 + head );
      } // heads
    } // cylinders
  } // units
}

// ===================================================================================================
//              P       A       Y               L       O       A       D               H A N D L E R
// ===================================================================================================
void
payload_handler(){
  int i, slots=rib.count/4; // four PDP10 words per slot
  if ( rib.count==0 ) return;

  // Directory content from the UFD files named *.UFD[1,1]
  if (  rib.prj==021 &&
        rib.prg==021 &&
        rib.ext==0654644 ){
    
    // each slot of the MFD file "  1  1.UFD[1,1]" represents a PPN sub-directory
    if ( rib.filnam == 021000021L ) ppncnt = slots;
    
    memcpy((void *)&ufd, // Each track 2304 words may have up to 576 UFDs
           (void *)&record,
           slots * sizeof(UFD_t));
    // copy new slots into the global slot table
    for( i=0; i<slots; i++ ){
      UFDslot[ 1 + ufdcnt + i ] = ufd[ i ];
    }
    // Tally-Ho-Update the number of file-slots seen so far within UFD directory files.
    ufdcnt += slots;
  }
}

// ===================================================================================================
//            R       E       A       D                 F       I       L       E
// ===================================================================================================
// read_file follows the chain of groups to build a track table,
// starting from the UFD-slot track's rib;
// which is the file's "prime_rib" of its group #1.
void
read_file(int slot){
  UFD_t ufd = UFDslot[ slot ];
  RIB_t prime;
  int track = ufd.track;
  // int prot  = ufd.prot;
  char prg[4], prj[4], filnam[8], ext[4], dir[32], file_name[32], path[128];
  char neo_mode_code="T1234567X9abcdeB"[ufd.mode]; // mode 0=T 8=X 15=B codes
  //
  int cre_date =  ufd.creation_date;
  int wrt_date = (ufd.date_written_high << 12) | ufd.date_written_low;
  int wrt_time =  ufd.time_written;
  //
  char created[18]; // "1974-07-26"
  char written[18]; // "1974-07-26T12:34"
  iso_date( created, cre_date, 0 );        created[10]=0;
  iso_date( written, wrt_date, wrt_time);  written[16]=0;
  //
  read_track( track );
  prime = rib;
  sixbit_uint_into_ascii_(    filnam,   ufd.filnam, sixbit_fname );
  sixbit_halfword_into_ascii_(   ext,   ufd.ext,    sixbit_fname );
  sixbit_halfword_into_ascii_(   prj, prime.prj,    sixbit_ppn   );
  sixbit_halfword_into_ascii_(   prg, prime.prg,    sixbit_ppn   );
  // destination directory
  sprintf( dir,"./UCFS/%s.%s",prg,prj); omit_spaces( dir );
  if(stat( dir,&statbuf) && errno==ENOENT) mkdir(dir,0777);
  // Destination for data8 is   ./UCFS/PRG.PRJ/.FILNAM.EXT
  // Destination for  text is   ./UCFS/PRG.PRJ/FILNAM.EXT
  //     underbars for spaces     in the [1,1] and [2,2] filenames
  // AND swap_halves of filenames in the [1,1] and [2,2]
  //    ./UCFS/1.1/Filename.UFD is Programmer-code then Project-code dot "UFD"
  //    ./UCFS/2.2/Filename.UFD is Programmer-code then Project-code dot "PLN" or dot "MSG"
  if((rib.prj==021 && rib.prg==021) ||
     (rib.prj==022 && rib.prg==022)) {
    sprintf( file_name, "%3.3s%3.3s%s%s", filnam+3, filnam, (ufd.ext ? ".":""), ext );
    space_to_underbar( file_name );
  }else{
    sprintf( file_name, "%s%s%s", filnam, (ufd.ext ? ".":""), ext );
  }
  sprintf( path,   "%s/.%s", dir, file_name );  omit_spaces( path );    // data8
  
  // copy SAIL-WAITS file content from the CKD disk pack into model UCFS data8 path name
  {
    int o, q, byte_count;
    int i, words = prime.count;
    int filesize = 8*words; // byte_count
    int tt=0, tracks = words/2304 + (words%2304 ? 1:0);
    int gg=0, groups = tracks /32 + (tracks %32 ? 1:0);
    int *track_table = calloc(groups*32,sizeof(int));
    // pdp10_word *buf8 = (pdp10_word *)malloc(filesize);
    
    fprintf( stderr, "read_file %4d.  "
             "%-26s %6d words %3d group%s %4d track%s  created %s  written %s\n",
             slot, path, words,
             groups, groups==1 ?" ":"s",
             tracks, tracks==1 ?" ":"s",
             created, written );
    
    // populate the track table
    for ( gg=0; gg<groups; gg++ ){                      // foreach group
      for ( i=0; i<16 && rib.dptr[i]!=0LL; i++){        // foreach word of rib DPTR
        track_table[ tt++ ] = (int)LRZ( rib.dptr[i] );  //  left DPTR
        track_table[ tt++ ] = (int)RRZ( rib.dptr[i] );  // right DPTR
      }
      if(i==16) read_track( rib.DNXTGP ); // fetch next group
    }
    while(!track_table[tt-1]) tt--; // count adjustment
    if(!(tt == tracks)){
      fprintf(stderr,"tt=%d tracks=%d\n",tt,tracks);
    }
    assert( tt == tracks );

    // Write DATA8 version of file content into the kit UCFS tree
    o = open( path, O_CREAT | O_WRONLY, 0664 ); if(o<0)perr( path );
    for ( tt=0; tt < tracks; tt++ ){
      read_track( track_table[tt] );
      payload_handler();
      byte_count = ( tt+1 < tracks ) ? 8*2304 : filesize - tt*8*2304;
      q= write( o, record, byte_count ); if(q!=byte_count)perr("write");
      // memcpy( bufptr, record, byte_count ); bufptr += byte_count;
    }
    close( o );
    touch_file_modtime( path, written );
  }  
#if 1
  switch( neo_mode_code ){
    char pathtext[128], path_b[128], path_x[128];
  case 'T':
    sprintf( pathtext,"%s/%s", dir, file_name );  omit_spaces( pathtext); // text
    data8_into_utf8( path, pathtext );
    touch_file_modtime( pathtext, written );
    break;
  case 'X':
    sprintf( path_x,"%s/%s.x", dir, file_name );  omit_spaces( path_x );  // executible

    data8_into_dasm( path, path_x );
    touch_file_modtime( path_x, written );
    break;
  case 'B':
  default:
    sprintf( path_b,"%s/%s.b", dir, file_name );  omit_spaces( path_b );  // binary
    data8_into_octal( path, path_b );
    touch_file_modtime( path_b, written );
    break;
  }
#endif
}

#if 0
// JUNK YARD
// =================================================================================================
// TODO
        ● Re-use or delete code and comments from the Junk Yard.
        ● Improve the SAIL DART multi-kinds of time-date stamps. Time Lord Policy.
/*
  SAIL filenames FILNAM.EXT[PRJ,PRG] are mapped into linux friendly /PRGPRJ/.FILNAM.EXT
  note 1. Prefixed dot on FILNAM indicates data8 binary.
  note 2. The SAIL [ Project , Programmer ] postfixed is swapped and moved to the left.
  note 3. For [1,1] and [2,2] file names embedded spaces are converted to underbars.
*/        
/*
  RIB datetime stamps -- Creation and Written
  set to DART date for initial Reënactment disk images.
  char iso_access_datetime[32]; // zero for initial Reënactment disk images.
  char iso_dump_datetime[32];   // zero for initial Reënactment disk images.
  char file_datetime[32];       // set UCFS files to proper historical date
  Track payload and destination of this fragment within its file
*/
/*
  TBX neo MODE abuse TBX ( Tea-BoX )
  .t text UTF8; is limited to SAIL ASCII characters,
        omit SOS line numbers,
        omit ETV table-of-content ( unfortunately called "directory" inside the E source )
  .b binary; other than DMP, presentation may include svg, png, csv, jason, ogg and yaml.
  .x executible; DMP format, may have extension .OLD .SAV .TMP .numerals and the like )
*/
/*
  Reënacted ribs from the SAILDART archive have cleared-off
  the DART backup bookkeepping data
  that was painfully maintained by the authentic DART program
  that ran on historical WAITS on a regular schedule.

  The DART backup operation required considerable Stanford personel, human employee manual effort,
  to mount and dismount the reels of tape, to label the tapes correctly,
  and to place or fetch the reels from the racks holding many hundreds of tapes.

  SAIL WAITS files have SIX date-time stamps:
  ● UFD created
  ● UFD written
  ● RIB creation-date,
  ● RIB reference-date
  ● RIB written-date
  ● RUB dump-date
*/
if(0)
  if( rib.created || rib.refdate || rib.date_lo ){
    fprintf(stdout,"cre_date_%06llo ",(uint64)rib.created );
    fprintf(stdout,"wrt_date_%06llo ",(uint64)rib.date_lo );
    fprintf(stdout,"wrt_time_%06llo ",(uint64)rib.time );
    fprintf(stdout,"ref_date_%06llo ",(uint64)rib.refdate );
    fprintf(stdout,"ref_time_%06llo ",(uint64)rib.reftime );
    fprintf(stdout,"dmp_date_%06llo ",(uint64)rib.dumpdate );
    //  Ignore RIB date ( use the UFD date derived from csv RALF input )
    //  wrt_date_[rib.track] = rib.c_date;
    //  wrt_time_[rib.track] = rib.time;
  }
/*
  Set group, as serial number of the RALPH cluster group 0 to group N.
  Set track_offset, by scanning for a track match in the RIB group track tables.
*/
The UCFS model
==============
// SAIL filnam.ext [ project, programmer ]
// UNIX programmer . project / filnam.ext       for UTF8 format
// UNIX programmer . project /.filnam.ext       for DATA8 format

// Ten disk pack units x 800 cylinders x 19 tracks per cylinder = 152,000 tracks
// record datetimes from the UFD "dirent" slots in the [1,1]{PRG}{PRJ}.UFD files
// use track-date of file when arriving at that track and writing that file's content into UCFS
unsigned short wrt_date_[152000];
unsigned short wrt_time_[152000];
unsigned short     mode_[152000]; // T=000 X=010 B=017 Re-purpose the WAITS-mode for Text Executible Binary.

/*
  TRACK ZERO is the SAT TABLE
  =========================== paste fragment of ralf/track.d code
  struct SATHEAD // Storage Allocation Table, sacred track zero
  {
  ulong dskuse, lstblk, satid, satchk, badcnt, badchk;
  ulong[32-6] badtrk;
  }
  struct SAT // Storage Allocation Table, sacred track zero
  {
  ulong[45-32+6] badtrk;
  ulong idsat, dtime, ddate, p1off, p2off, spare;
  ulong[1267] satbit; // size of SAT bit table in SYSTEM.DMP[J17,SYS]
  ulong[2304-1267-57] unused; // 980. words of padding
  }
  ===========================
  so for this program, the SAT bit table is 1267 words long,
  and is found offset 57. words into track#0
*/

  /*
    MFD directory   is  at   1.1/__1__1.UFD
    UFD directories are like 1.1/BGB__1.UFD        swap [ Project, Programmer ] for UCFS unix file system eyeballs
    --
    User directories were  kept in PRJPRG.UFD[1,1] sailfiles with spaces allowed       in the FILNAM {project}{programmer}
    User directories now placed in 1.1/prgprj.UFD  unixfiles with underbar replacing space in FILNAM {programmer}{project}
    sixbit/UFD/ == 0654644 is a well known constant for the extension "UFD" of a sub-directory filename.
    --
    When inside the root directory [1,1] and so
    also inside a {ppn}.UFD file
    then copy the UFD-slots for later file-track-following.
  */

  /* Pontification on Nomensclature:

    The verbs "load" and "unload" are "container centric",
    as well as over used in the PDP10 WAITS context.
    Names (sentences) starting with verbs for data conversion such as
    load/unload, dump/save, pack/unpack/repack,
    translate, convert, encode/decode, mix, make, build
    usually fail to specify both SOURCE and DESTINATION.

    Mnemonic silliness, CKD into Chickadee, is justified
    because it provides a vivid mnemonic for the IBM disk pack format
    and it removes a TLA, Three-Letter-Acronym, from our playing field.

    I wish to avoid the tag "SYS" as much as possible.
    So the simulated IBM-3330 pizza oven that holds a number of disk packs will be called DASD;
    and an instance of the SAIL-WAITS file system (the Ralph or Ralf format) is a Disk-Kit;
    with a T-shirt-size dashed-name.

    I had previously named the predecessor to such kits as SYS000 to SYS999,
    which was a mistake.
    I have tolerated bash environment variables named "SYS*" for my working UCFS path(s).

    echo $SYS
    /home/bgb/waits-disk-kits/tiny-IDE/UCFS

    UCFS stands for Upper-Case-File-System,
    which is the dual-content model of a
    SAIL-WAITS Ralfs (a ralph verified File System).

    And UCFS reminds me of UCSF,
    the University of California San Francisco,
    and all that poison oak on nearby Mount Sutro.
  */

#endif

// ===================================================================================================
//                              M       A       I       N
// ===================================================================================================
int
main(int ac, char **av){
  char *kitpath;
  int unit, slot;
  char unit_ckd[32];
  if(ac < 2){
    fprintf(stderr,"\nsynopsis: %s kitpath\n\n",av[0]);
    return 1;
  }
  flag_into_tracks = strstr(av[0],"tracks") != NULL;
  flag_into_UCFS = strstr(av[0],"UCFS") != NULL;  
  kitpath = av[1];
  if(chdir(kitpath)) perr("chdir");
  if( stat( "./0.ckd", &statbuf ) && errno==ENOENT){
    fprintf(stderr,"\n"
            "   ./0.ckd NOT found in your current working directory !\n\n"            
            "   The first disk-pack, \"./0.ckd\" was NOT found.\n"
            "   Your current-working directory must be the WAITS DISK KIT you wish to re-build.\n"
            "   AND you must first run DASD_into_chickadee to make disk pack files\n"
            "   BEFORE you can make (or re-new) the kit's UCFS file system.\n\n"
            );
    exit(1);
  }
  if(stat("./UCFS",  &statbuf) && errno==ENOENT) mkdir("./UCFS", 0777);
  if(stat("./track", &statbuf) && errno==ENOENT) mkdir("./track",0777);    //
  //
  SAT[0] = 0x800000000L; // Allocate track#0 initialization mickey mouse
  csv = fopen("./ckd.csv","w");
  logr= fopen("./ckd.log","w");
  // open all the disk units that can be found in this kit
  strcpy( unit_ckd, "./0.ckd" );
  for( unit=0; unit<=9; unit++ ){
    disk[unit] = fopen( unit_ckd, "r" );
    if(!disk[unit]) break;
    unit_ckd[strlen(unit_ckd)-5]++;
  }
  read_track( 0 );              // Read SAT table from track #0.
  SAT_and_MFD_initialization(); // Allocate UFDslot space. Place MFD seed into UFDslot[1].
  read_file( 1 );               // track #1 has the first (up to 576) UFDslots of directory files.
  if(1)
  for( slot=2; slot <= ufdcnt; slot++){
    read_file( slot );
  }
  fprintf(stderr,"ppncnt=%d ufdcnt=%d ufdpos=%d\n",ppncnt,ufdcnt,ufdpos);
  return(0);
}
/*
  Print command at my house `2021-05-14 bgbaumgart@mac.com'
  lpr -P Black1 -o media=legal -o page-left=48 -o cpi=14 -o lpi=8 chickadee_into_UCFS.c
  ----
  EOF.
*/
