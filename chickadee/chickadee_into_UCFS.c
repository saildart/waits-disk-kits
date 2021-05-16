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

#define perr(msg) if(1){perror(msg);exit(EXIT_FAILURE);}
#define bamboo // message clutter management markers
#define bamoff if(0)
#define bamon  if(1)

typedef unsigned char      uint8; // bit width type names
typedef unsigned short int uint16;
typedef unsigned       int uint32;

/* ================================================================================
           G       L       O       B       A       L
   ================================================================================
   Warning: Too much global state. Funky use of uppercase names SAT and UFD.
   SAIL-WAITS jargon does not carefully distinguish UFD-slots, UFD-files and PPNs.

   notes:
   Every SAIL-WAITS file had a UFD-slot inside a UFD-file.
   UFD-files were named ↓*.UFD[1,1]↓ and contained directories of UFD-slots.
   The root directory was named ↓  1  1.UFD[1,1]↓ and had one-slot for each PPN.
   PPN is a Project-Programmer-Name for example [1,BGB]
*/
RIB_t rib;      // Thirty-two PDP10 words, followed by 2304. PDP10 words.
UFD_t ufd[576]; // four PDP10 words per UFD dir entry. 2304./4 = 576. per track.
UFD_t *UFDslot; // Allocated by riblock_handler. Filled in by the payload_handler.

int ufdpos=1; // next UFD-slot for the file-track-follower.
int ufdcnt=0; // total number of files seen over all [*,*] directories
int ppncnt=0; // total number of UFD files in the [1,1] root directory

uint64 record[18*128];  // 2304. PDP10 words per track
uint64 SAT[1267];       // WAITS sacred Storage-Allocation-Table bit map of used tracks.
SATHEAD_t sathead;
SAT_t     satbody;
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

// Ten disk pack units x 800 cylinders x 19 tracks per cylinder = 152,000 tracks
// record datetimes from the UFD "dirent" slots in the [1,1]{PRG}{PRJ}.UFD files
// use track-date of file when arriving at that track and writing that file's content into UCFS
unsigned short wrt_date_[152000];
unsigned short wrt_time_[152000];
unsigned short     mode_[152000]; // T=000 X=010 B=017 Re-purpose the WAITS-mode for Text Executible Binary.

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

// Forward declarations - outline the program
void pass0(); // READ the  __1__1.UFD[1,1] build directory PPN table   ppnames
void pass1(); // READ each      *.UFD[1,1] build   normal FILE table   fenames
void pass2(); // READ SAIL files by following track links from UFD track locus
void pass_ex(); // exercise sequentially read all the tracks
// ===================================================================================================
//                              M       A       I       N
// ===================================================================================================
int
main(int ac, char **av){
  char *kitpath;
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
            "   BEFORE you can make a new UCFS file system.\n\n"
            );
    exit(1);
  }
  if(stat("./UCFS",  &statbuf) && errno==ENOENT) mkdir("./UCFS", 0777);
  if(stat("./track", &statbuf) && errno==ENOENT) mkdir("./track",0777);
  pass0();
  fprintf(stderr,"ppncnt=%d ufdcnt=%d ufdpos=%d\n",ppncnt,ufdcnt,ufdpos);
  return(0);
}

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
  cyl  = ( track % 15200 ) / 800;
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
  assert( cyl == cylinder   );
  assert(  hd == head       );
  assert(  -1 == end_record ); // EOR mark confirmed.
  
  // Format two PDP10 words from 9-bytes into 16-bytes, step past 13 byte "start-record".
  data9_into_data8_cromwell( rib_, (data8_word_t *)&rib, 32 );
  // from tracks made up from 18 IBM records, each with an 8 byte prefix to step over.
  for(i=0;i<18;i++){
    data9_into_data8_cromwell( payload_ + 8 + i*(8+64*9),
                               (data8_word_t *)(&record[i*128]),
                               128 );
  }  
  // Once only track zero processing
  if (track==0){
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
  }
  // IBM hardware address
  fprintf( stdout,
           "read_track(%d) %d.pack %3d.cylinder %2d.head %6d.track  filnam %012lo ext %06o rib.track=%d rib.count=%d\n",
           track, unit, cylinder, head, track,
           (ulong)rib.filnam, rib.ext, (int)rib.track, (int)rib.count );
}
// ===================================================================================================
//            R       E       A       D                 F       I       L       E
// ===================================================================================================
// Read file by following its tracks starting from its UFD slot
void
read_file(int track){
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
//                                      R       I       B
// ===================================================================================================
void
riblock_handler(int track){
  int i, track_offset=-1, tnext=0;
  int group, group_next;
  // int wrdcnt = rib.count; // 'DDLNG' from RIB
  if (track==0 || rib.count==0) return; // nothing to do here        
  group =       rib.DGRP1R / 576; // 576=18*32
  group_next =  rib.DNXTGP;
  track_offset = -1;
  for(i=0; i<16 && rib.dptr[i]!=0LL; i++){
    if(track == (int)LRZ(rib.dptr[i])){
      track_offset = group*32 + 2*i;
    }
    if(track == (int)RRZ(rib.dptr[i])){
      track_offset = group*32 + 2*i + 1;
      // byte_offset = track_offset * 8 * 2304;
      // byte_count = wrdcnt * 8;
    }
  }
  fprintf( stdout,
           "track x%06d  tnext x%06d  group=%d  group_next=%d   track_offset=%d\n",
           track,        tnext,       group,    group_next,     track_offset );
  for(i=0; i<16; i++){
    if(i%4==0)fprintf(stderr,"%2d/       ",2*i);else fprintf(stderr,"       ");
    fprintf(stderr, " %6lld %6lld    ",
            (uint64)LRZ(rib.dptr[i]),
            (uint64)RRZ(rib.dptr[i]));
    if(i%4==3)fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");
}
#if 0
  char prg[4], prj[4], filnam[8], ext[4]; // from RIB track  
  char file_name[32]; // underbars replace spaces for the [1,1] and [2,2] file names
  char dir[32]="", path[128]="";
  char pathtext[128];
  char path_b[128];
  char path_x[128];
  sixbit_uint_into_ascii_(    filnam, rib.filnam, sixbit_fname );
  sixbit_halfword_into_ascii_(   ext, rib.ext,    sixbit_fname );
  sixbit_halfword_into_ascii_(   prj, rib.prj,    sixbit_ppn   );
  sixbit_halfword_into_ascii_(   prg, rib.prg,    sixbit_ppn   );
  sprintf(dir,"./UCFS/%s.%s",prg,prj); omit_spaces( dir );
  if(stat(dir,&statbuf) && errno==ENOENT) mkdir(dir,0777); // destination directory
  // destination FILNAM.EXT
  // underbar_spaces in [1,1] and [2,2] filenames
  // AND swap_halves
  // for UCFS the Filename.UFD is Programmer-code then Project-code.
  {
    if((rib.prj==021 && rib.prg==021) ||
       (rib.prj==022 && rib.prg==022)) {
      sprintf( file_name, "%3.3s%3.3s%s%s", filnam+3, filnam, (rib.ext ? ".":""), ext );
      space_to_underbar( file_name );
    }else{
      sprintf( file_name, "%s%s%s", filnam, (rib.ext ? ".":""), ext );
    }
    sprintf( path,   "%s/.%s", dir, file_name ); // data8
    sprintf( pathtext,"%s/%s", dir, file_name ); // text8
    sprintf( path_b,"%s/%s.b", dir, file_name ); // binary
    sprintf( path_x,"%s/%s.x", dir, file_name ); // executible
    omit_spaces( path );
    omit_spaces( pathtext );
    omit_spaces( path_b );
    omit_spaces( path_x );
  }
#endif

// ===================================================================================================
//                      P       A       Y               L       O       A       D
// ===================================================================================================
void
payload_handler(int track){
  int i;
  // char prg[4], prj[4], filnam[8], ext[4]; // from RIB track
  // char filnam[8];
  // char fnam[8], extn[4];                  // from UFD file in [1,1] area
  // char iso_creation_date[32];           // NOT unix  ctime, which is Change inode time stamp.
  // char iso_written_datetime[32];        // Like unix mtime, file Modification time stamp.
  // SAT from track zero - ONCE only.
  if (track==0 || rib.count==0) return; // nothing to do here
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
  if (  rib.prj==021 &&
        rib.prg==021 &&
        rib.ext==0654644 ){    
    int slots = rib.count / 4;          // four PDP10 words per slot
    if ( rib.filnam == 021000021L ){    // MFD at 1.1/__1__1.UFD ≡ ↓  1  1↓.UFD[1,1]
      ppncnt = slots;                   // each slot of the MFD file represents a PPN sub-directory
    }
    memcpy(     (void *)&ufd,           // This track has up to 576. UFDs ≡ 2304. words ≡ 18432. bytes
                (void *)&record,
                slots * sizeof(UFD_t));
    // copy these new slots into the global slot table
    for( i=0; i<slots; i++ ){
      UFDslot[ 1 + ufdcnt + i ] = ufd[ i ];
    }
    ufdcnt += slots; // Tally-Ho-Update the number of file-slots seen so far within the UFD directory files.
  }  
#if 0
    // track payload holds up to 2304. PDP10 words, room for 576. four-word UFD entries.    
    for( i=0; i<576; i++ ){
      int mode, prot, xtrack;
      int cre_date, wrt_date, wrt_time;
      
      if( ufd[i].filnam==0 ) continue; // skip the empty slots
      
      sixbit_uint_into_ascii_(     fnam, ufd[i].filnam, sixbit_fname );
      sixbit_halfword_into_ascii_( extn, ufd[i].ext,    sixbit_fname );
      space_to_underbar( fnam ); // Preservation for [1,1] and [2,2] file names with leading spaces.
            
      cre_date =  ufd[i].creation_date;
      wrt_date = (ufd[i].date_written_high << 12) | ufd[i].date_written;
      wrt_time =  ufd[i].time_written;            
      iso_date( iso_creation_date,    cre_date, 0 );  iso_creation_date[10]=0;
      iso_date( iso_written_datetime, wrt_date, wrt_time);  iso_written_datetime[16]=0;
            
      mode = ufd[i].mode;
      prot = ufd[i].prot;
      xtrack=ufd[i].track;            
      fprintf(stdout,
              "        %3d user %s / %s.%s   %2o.mode %03o.prot track x%06d written %s created %s\n",
              i, filnam,
              fnam, extn, mode, prot,
              xtrack,
              iso_written_datetime,
              iso_creation_date );
      wrt_date_[xtrack] = wrt_date;
      wrt_time_[xtrack] = wrt_time;
      mode_[xtrack] = mode;
    }            
  }
#endif
#if 0
  // place payload into data8 file
  if( flag_into_UCFS )
    {
      off_t byte_offset;
      size_t byte_count;
      size_t bytes_remaining;
      uint32 address;
      char neo_mode_code;
      //
      if(!o){            
        neo_mode = mode_[track];
        neo_mode_code = "T1234567X9abcdeB"[neo_mode & 017]; // modes 0=T 8=X 15=B codes
        if(!wrt_date_[track]){
          wrt_date_[track] = rib.date_lo | (rib.date_hi<<12);
          wrt_time_[track] = rib.time;
        }
        iso_date( file_datetime, wrt_date_[track], wrt_time_[track] );
        o = open( path, O_CREAT | O_WRONLY, 0664 ); if(o<0)perr( path );
        fprintf(stdout,"    open %28s date %s\n",path,file_datetime);
        strcpy( oldpath, path );
        fprintf(csv,"%3s.%3s %-10s,%8d,%c,000000,  %-30s,%s\n",
                prg,prj,file_name,
                wrdcnt, neo_mode_code,
                path,   file_datetime);
      }else{
        ;//fprintf(stdout,"continue %s\n",path);
      }
      address = track_offset * 2304;
      byte_offset = track_offset * 2304 * 8;
      bytes_remaining = wrdcnt*8 - byte_offset;
      byte_count =  bytes_remaining <= 2304*8 ? bytes_remaining : 2304*8;
      bamoff fprintf(stdout,"   write %ld bytes at addr %06o lseek %6ld; path %s\n",
                     byte_count, address, byte_offset, path );
      q= lseek( o, byte_offset, SEEK_SET );  if(q<0) perr("seek");
      q= write( o, record, byte_count );     if(q!=byte_count)  perr("write");         
    }
#endif        
#if 0
  // Final Fragment of data8 version.
  // final track of the file
  if( bytes_remaining <= 2304*8 ){
    q = close( o );
    o = 0;
    touch_file_modtime( path, file_datetime );
    bamboo fprintf(stdout,"   close %s date %s\n\n", path, file_datetime );
    switch( neo_mode ){
    case 0:
      data8_into_utf8( path, pathtext );
      touch_file_modtime( pathtext, file_datetime );
      break;
    case 010:
      data8_into_dasm( path, path_x );
      touch_file_modtime( path_x, file_datetime );
      break;
    case 017:
      data8_into_octal( path, path_b );
      touch_file_modtime( path_b, file_datetime );
      break;
    }
  }
#endif
}

void
pass0(){
  int unit;
  char unit_ckd[40]; // file names $KIT/0.ckd through $KIT/9.ckd
  //  
  SAT[0] = 0x800000000L; // allocate track#0 initialization mickey mouse
  csv = fopen("./ckd.csv","w");
  logr= fopen("./ckd.log","w");
  // open all disk units found in the kit
  strcpy(unit_ckd,"./0.ckd");
  for(unit=0;unit<=9;unit++){
    disk[unit] = fopen( unit_ckd, "r" );
    if(!disk[unit]) break;
    unit_ckd[strlen(unit_ckd)-5]++;
  }
  // read SAT table from track #0
  read_track(0);
  riblock_handler(0);
  payload_handler(0);
  // read MFD  file "  1  1.UFD[1,1]" starting at track #1
  read_track(1);
  riblock_handler(1);
  payload_handler(1); // goes into  UCFS/1.1/.__1__1.UFD
}

void
pass1(){
}

void
pass2(){
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

   The Cornwell SIMS IBM disk format has () additional bytes
   for address markers
   and zero padding.

   And there is an additional 512. byte IBM hardare pack label on the front of the CKD file.
*/
void
pass_ex(){
  //  int i,j,o=0,q=0;
  struct pmp_header hdr;
  int unit,cyl,cylinder=-1,hd,head=-1;  
  int track;
  //  int group, group_next;
  //
  for(unit=0;unit<=9;unit++){
    fseek(disk[unit], 0, SEEK_SET);
    fread(&hdr, sizeof(struct pmp_header),1,disk[unit]);
    
    // IBM hardware pack label
    bamoff
      fprintf(stdout,
              "devid '%8.8s'  == 'CKD_P370' ?\n"
              "heads     %6d  ==    19. ?\n"
              "tracksize %6d  == 13312. ?\n"
              "dev type    0x%0x  ==  0x30  ?\n"
              "highcyl   %6d  ==   815. ?\n",
              hdr.devid,
              hdr.heads,
              hdr.tracksize,
              hdr.devtype,
              hdr.highcyl );
  
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
        bamoff fprintf(stdout,"%d.pack %3d.cylinder %2d.head %6d.track\n",
                       unit, cylinder, head, track); // IBM hardware address        
        track = ( unit*800*19 + cylinder*19 + head );
        
        // SAT check - no need to look at unallocated tracks
        if (!(SAT[track/36] & (0x800000000L >> track%36))) continue;
        
        riblock_handler( track );
        payload_handler( track );
      } // heads
    } // cylinders
  } // units
}

// JUNK heap - re-use or delete code/comments - AND improve the date handling policy.
// =================================================================================================
#if 0
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
  neo MODE abuse
  .t text UTF8; is limited to SAIL ASCII characters,
  omit SOS line numbers,
  omit ETV table-of-content (wrongly called "directory" inside E source)
  .b binary; other than DMP, presentation may include svg, png, csv, jason, ogg and yaml.
  .x executible; DMP format, may have extension .OLD .SAV .TMP .numerals and the like )
*/
/*
  Reënacted ribs from the SAILDART archive have cleared-off
  the DART backup bookkeepping data
  that was painfully maintained by the authentic DART program
  that ran on historical WAITS on a regular schedule.

  The DART backup runs required considerable Stanford personel, human employee manual effort,
  to mount and dismount the reels of tape, and to label the tapes correctly,
  and to place or fetch the reels from the racks holding the many hundreds of tapes.

  So creation date, reference date and written date files non-zero;
  indicates recent 21st century (or beyond) simulated WAITS activity on the file.
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
  Set group, as serial number of the RALPH cluster group 0 to N.
  Set track_offset, by scanning for a track match in the RIB group track table.
*/
The UCFS model
==============
// SAIL filnam.ext [ project, programmer ]
// UNIX programmer . project / filnam.ext       for UTF8 format
// UNIX programmer . project /.filnam.ext       for DATA8 format
#endif
/*
  Print command at my house `2021-05-14 bgbaumgart@mac.com'
  lpr -P Black1 -o media=legal -o page-left=48 -o cpi=14 -o lpi=8 chickadee_into_UCFS.c
  ----
  EOF.
*/
