#if 0 // unload_chickadee.c -*- mode:C; coding:utf-8; -*-
NAME="unload_ckd"
SRC="/KIT/src"
DST="/KIT/bin"
#           -Wall
gcc -g -m64       -Werror -o $DST/$NAME  $SRC/$NAME.c && echo OK || echo FAILED
echo $DST/$NAME
return 2>/dev/null || exit 0
#endif
// gcc -g -o unload_ckd unload_ckd.c
// `2020-07-25 bgbaumgart@mac.com'

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "/data/KA10sn32/library/kayten.c"

#define perr(msg) if(1){perror(msg);exit(EXIT_FAILURE);}

// debug message clutter management
#define bamboo
#define bamoff 0&&
#define bamon  1&&

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;

// model of Petit IBM track
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

uint8 start_record[13];
uint8 rib_[144];
uint8 payload_[10512];
int   end_record;
uint8 zero_padding[2783];

int timestamp=0;

RIB_t rib;              // Thirty-two PDP10 words, followed by 2304. PDP10 words.
UFDent_t ufd[576];      // four PDP10 words per UFD dir entry. 2304./4 = 576. per track.

uint64 record[18*128]; // 2304. PDP10 words per track
/*
  DATA8 has one PDP-10 word placed in 64-bits right adjusted.
  DATA9 has two PDP-10 words packed into 9 bytes
  The Cornwell CKD version of DATA9 packing is called "DeadBeef56DeadBeef"
  because two PDP10 word bit-pattern 0xDeadBeef5 and 0xDeadBeef6
  are pack into 9 bytes as 0xDeadBeef56DeadBeef.
*/
#if 0
typedef union {
  // Little endian x86 struct
  struct { uint64  byte1:8, byte2:8, byte3:8, byte4:8, nibble_lo:4,:28; } even;
  struct { uint64 nibble_hi:4, byte6:8, byte7:8, byte8:8, byte9:8, :28; } odd;
} data8_word_t;
#endif
// Two PDP10 words deadbeef5 deadbeef6 <<==>> CKD deadbeef56deadbeef bit pattern.
typedef union {
  // Little endian x86 struct
  struct { uint64 nibble_hi:4, byte4:8, byte3:8, byte2:8, byte1:8, :28; } even;
  struct { uint64 nibble_lo:4, byte9:8, byte8:8, byte7:8, byte6:8, :28; } odd;
} data8_word_t;

void
repack_data9_into_data8(uint8 *data9, data8_word_t *data8, int wrdcnt ){
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

/* P A S S 1
  --------------------------------------------------------------------------- 80 ...... 90 ..... 100
  Linear scan of the Cornwell SYS000.ckd disk images input from the current directory.
  Scan 1 to 8 packs, 800 cylinders, 19 heads; net 15200 tracks per pack.
  Each track header is a 32 PDP10 word RIB, Retrieval Information Block,
  followed by up to 2304. PDP10 words of SAIL-WAIT file content.
  The Cornwell SIMS IBM disk format has further bytes for address markers and zero padding.
 */
void
pass1(){
  FILE *disk,*csv;
  int o=0,q=0;
  struct pmp_header hdr;
  char name[40];
  int i,j;

  int unit,cyl,cylinder=-1,hd,head=-1;
  int track, track_offset;
  int group, group_next;
  int MFD_flag; // UFD file in directory [1,1] with extension UFD, sixbit/UFD/==0654644
  /*
        SAIL filenames FILNAM.EXT[PRJ,PRG] are mapped into linux friendly ./SYS/PRGPRJ/.FILNAM.EXT
        note 1. Prefixed dot on FILNAM indicates data8 binary.
        note 2. The SAIL [ Project , Programmer ] postfixed is swapped and moved to the left.
  */
  char fnam[8], extn[4];                                // from UFD file in [1,1] area
  char prg[4], prj[4], filnam[8], ext[4];       // from RIB track
  char ppname[32]="";
  char dir[32]="",path[128]="",oldpath[64]="";
  char userpath[40];
  //
  // RIB datetime stamps -- Creation and Written set to DART date for initial Reënactment disk images.
  //
  char iso_creation_date[32];           // NOT unix  ctime, which is Change inode time stamp.
  char iso_written_datetime[32];        // Like unix mtime, file Modification time stamp.
  char iso_access_datetime[32];         // zero for initial Reënactment disk images.
  char iso_dump_datetime[32];           // zero for initial Reënactment disk images.
  //
  // Track payload and destination of this fragment within its file
  //
  int wrdcnt, locus; // from RIB
  //
  // output
  //
  struct stat statbuf;
  char zip288[288]="";
    
  csv = fopen("ckd.csv","w");
  strcpy(name,"SYS000.ckd");

  if(stat("SYS",  &statbuf) && errno==ENOENT) mkdir("SYS",  0777);
  if(stat("track",&statbuf) && errno==ENOENT) mkdir("track",0777);
  
  for(unit=0;unit<2;unit++){
    disk = fopen(name, "r");
    if(!disk)perr(name);    
    fseek(disk, 0, SEEK_SET);
    fread(&hdr, sizeof(struct pmp_header),1,disk);
    
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
        if(1!=fread( &start_record,          13 ,   1,disk)) perr("start record");      //     13. bytes
        if(1!=fread( &rib_,                16*9 ,   1,disk)) perr("rib");               //    144. bytes
        if(1!=fread( &payload_,      18*(8+64*9),   1,disk)) perr("payload");           // 10,512. bytes
        if(1!=fread( &end_record,             4 ,   1,disk)) perr("end record");        //      4. bytes
        if(1!=fread( &zero_padding, 13312-(13+144+144+10368+4),                         //  2,639. bytes
                                                    1,disk)) perr("zero padding");

        // Confirm track format compliance.
        cylinder = start_record[1]<<8 | start_record[2];
        head     = start_record[3]<<8 | start_record[4];
        assert( cyl == cylinder   );
        assert(  hd == head       );
        assert(  -1 == end_record ); // EOR mark confirmed.
        track = ( unit*800*19 + cylinder*19 + head );
        
        // debug IBM hardware address
        bamoff fprintf(stdout,"%s %d.pack %3d.cylinder %2d.head %6d.track\n",name,unit,cylinder,head,track);

        // TRACK content
        // unpack content from rib_ and payload_
        repack_data9_into_data8( rib_, (data8_word_t *)&rib, 32 );
        for(i=0;i<18;i++) repack_data9_into_data8( payload_+8+i*(8+64*9), (data8_word_t *)(&record[i*128]), 128 );
        /*
            The payload_ bytes contain 18. records with
            8 bytes of IBM disk-head-address prefixed to
            64*9 bytes of packed PDP10 data.

            Nine bytes of packed data hold two PDP10 words.
            The Cornwell packing is done by ~/sky/load_ckd source by the file named load_ckd.c
            Also see my remarks at ./deadbeef56deadbeef.note

            There are 18*128 = 2304. PDP10 words of SAIL-WAITS data per track
            which is extracted from the payload_ bytes.
        */
        
        // write track, octal dump format
        if(0){
          char track_xnumber[40]; // track/x000999 files octal data13
          FILE *tfile;
          uint64 *ptr;
          sprintf( track_xnumber, "track/x%06d", track );        
          tfile = fopen( track_xnumber, "w" );
          ptr=(uint64 *)&rib;
          for(i=0;i<32;  i++) fprintf(tfile,"%012llo\n",ptr[i]);
          for(i=0;i<2304;i++) fprintf(tfile,"%012llo\n",record[i]);
          fclose(tfile);
        }       
        
        // User directories were  kept in PRJPRG.UFD[1,1] sailfiles with spaces allowed       in the FILNAM {project}{programmer}
        // User directories now placed in 1.1/prgprj.UFD  unixfiles with underbar replacing space in FILNAM {programmer}{project}
        // The MFD directory  is  at      1.1/__1__1.UFD
        MFD_flag = ( rib.prj==021 && rib.prg==021 && rib.ext==0654644 ); // sixbit/UFD/==0654644 well known constant.
        if( MFD_flag ){
          for(i=0;i<18;i++)
            repack_data9_into_data8( payload_+8+i*(8+64*9),
                                     (data8_word_t *)(&ufd[i*32]), 128 );
        }
        
        // Repack RIB content into file name path strings
        wrdcnt = rib.count;     // Ralph SAIL-WAITS name 'DDLNG' length in words.
        locus  = rib.track;     // track (offset 0) of file, name 'DDLOC' location of first track of the file.        
        if(!wrdcnt)continue;    // Early exit for empty tracks (and track zero the SAT table)
        
        // SAIL filnam.ext [ project, programmer ]
        // UNIX programmer . project / filnam.ext       for UTF8 format
        // UNIX programmer . project /.filnam.ext       for DATA8 format
        sixbit_uint_into_ascii_(    filnam, rib.filnam, sixbit_fname );
        sixbit_halfword_into_ascii_(   ext, rib.ext,    sixbit_fname );
        sixbit_halfword_into_ascii_(   prj, rib.prj,    sixbit_ppn   );
        sixbit_halfword_into_ascii_(   prg, rib.prg,    sixbit_ppn   );
        
        // destination directory
        sprintf(  dir, "SYS/%s.%s", prg, prj );
        omit_spaces( dir );
        if(stat(dir,&statbuf) && errno==ENOENT) mkdir(dir,0777);

        // destination FILNAM.EXT with special handling for embedded spaces in [1,1] and [2,2] filenames
        {
          char name[32];
          int flag = (rib.prj==021 && rib.prg==021) || (rib.prj==022 && rib.prg==022);
          if(flag){
            sprintf( name, "%3.3s%3.3s%s%s",
                     filnam + ( flag ? 3 : 0 ), // for UNIX swap-halves so Filename.UFD is Programmer-code then Project-code.
                     filnam + ( flag ? 0 : 3 ),
                     (rib.ext ? ".":""), ext );
            space_to_underbar( name );
          }else{
            sprintf( name, "%s%s%s", filnam, (rib.ext ? ".":""), ext );
          }
          sprintf( path, "%s/.%s", dir, name );
          omit_spaces( path );
        }
        
        // Reënacted ribs lack the bookkeepping data written by the authentic DART on WAITS.
        if(0){
          fprintf(stdout,"%lld.c_date ",   (uint64)rib.c_date );
          fprintf(stdout,"%lld.date_lo ",  (uint64)rib.date_lo );
          fprintf(stdout,"%lld.time ",     (uint64)rib.time );
          fprintf(stdout,"%lld.refdate ",  (uint64)rib.refdate );
          fprintf(stdout,"%lld.reftime ",  (uint64)rib.reftime );
          fprintf(stdout,"%lld.dumpdate ", (uint64)rib.dumpdate );
        }
        
        // Set group, as serial number of the RALPH cluster group 0 to N.
        // Set track_offset, by scanning for a track match in the RIB group track table.
        group = rib.DGRP1R / 576; // 576=18*32
        group_next = rib.DNXTGP;
        bamoff fprintf(stdout,"group=%d next=%d   ", group, group_next );
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
        
        if( track_offset < 0 ){
          //  fprintf(stderr,"\nFinal track+1 is x%06d. End of diskpack named %s.\n",track,name);continue;
          fprintf(stderr,"\nBAD RETREIVAL %s\n current track x%06d not found in RIB DDPTR table\n",
                  path,
                  track);
          // Debug octal dump of the rib cluster group table of track pointers
          if(1) for(i=0; i<16 && rib.dptr[i]!=0; i++){
              fprintf(stderr,"%lld %lld ", (uint64)LRZ(rib.dptr[i]), (uint64)RRZ(rib.dptr[i]));
          }
          fprintf(stderr,"\n\n");
          continue;
        }
        
        // debug IBM hardware address with track_offset
        bamboo fprintf(stdout,"%s %d.pack %3d.cylinder %2d.head %6d.track %4d.offset\n",
                       name,unit,cylinder,head,track,track_offset);
        bamboo fprintf(stdout,"x%06d   %3d, %6d, %-24s.\n",
                       track, track_offset, wrdcnt, path );
        
        /*
          The Master File Directory starts on track#1.
          The file named __1__1.UFD[1,1] is always the first UFD slot on track#1
          followed by one four word UFD for each user Project Programmer directory.
        */
        if( MFD_flag ){
          for( i=0; i<576; i++ ){ // track payload holds 2304. PDP10 words, room for 576. four-word UFD entries.
            int mode, prot, locus_from_UFD;
            int cre_date, wrt_date, wrt_time;
            if( ufd[i].filnam==0 ) continue; // skip the empty slots
            
            sixbit_uint_into_ascii_(     fnam, ufd[i].filnam, sixbit_fname );
            sixbit_halfword_into_ascii_( extn, ufd[i].ext,    sixbit_fname );
            space_to_underbar( fnam ); // Preservation for [1,1] and [2,2] file names with leading spaces.
            cre_date = ufd[i].creation_date;
            wrt_date = ufd[i].date_written_high << 12 || ufd[i].date_written;
            wrt_time = ufd[i].time_written;            
            iso_date( iso_creation_date,    cre_date, wrt_time);  iso_creation_date[16]=0;
            iso_date( iso_written_datetime, wrt_date, wrt_time);  iso_written_datetime[16]=0;
            
            // iso_date(iso_adatetime,adate,atime);  iso_adatetime[10]=' ';      iso_adatetime[16]=0;
            // iso_date(iso_ddatetime,ddate,0);      iso_ddatetime[10]=0;
            mode = ufd[i].mode;
            prot = ufd[i].prot;
            locus_from_UFD= ufd[i].track;
            // Upper Case linux file name path representation of a SAIL-WAITS filename.
            // Note Programmer.Project prefixed.
            // Note extra prefixed .FILNAM dot indicates the binary data8 format.
            sprintf( userpath,
                     "SYS/%3.3s.%3.3s/.%3.3s%3.3s%s%s",
                     filnam+3,filnam,
                     fnam+3,fnam,
                     extn?".":"",
                     extn);
            omit_spaces( userpath );
            bamoff fprintf(stdout,
                           "        %3d user %s / %s.%s   %2o.mode %03o.prot %6d.locus created %s userpath %s\n",
                           i, filnam,
                           fnam,extn,mode,prot,locus_from_UFD,
                           iso_creation_date,
                           userpath );
          }            
        }        
        
        // place track payload into data8 file
        {
          off_t byte_offset;
          size_t byte_count;
          size_t bytes_remaining;
          uint32 address;
          //
          if(!o){
            o = open( path, O_CREAT | O_WRONLY, 0664 ); if(o<0)perr( path );
            fprintf(stdout,"    open %s\n",path);
            strcpy( oldpath, path );
          }else{
            fprintf(stdout,"continue %s\n",path);
          }
          address = track_offset * 2304;
          byte_offset = track_offset * 2304 * 8;
          bytes_remaining = wrdcnt*8 - byte_offset;
          byte_count =  bytes_remaining <= 2304*8 ? bytes_remaining : 2304*8;
          //
          bamboo fprintf(stdout,"   write %ld bytes at addr %06o lseek %6ld; utime %d; path %s\n",
                         byte_count,
                         address,
                         byte_offset,
                         timestamp, path );
          q = lseek( o, byte_offset, SEEK_SET );
          if(q<0){
            perr("seek");
          }
          q = write( o, record, byte_count );
          if(q!=byte_count){
            perr("write");
          }
          if( bytes_remaining <= 2304*8 ){
            q = close( o );
            o = 0;
            bamboo fprintf(stdout,"   close %s\n\n",path);
          }
        }
        
      } // heads
    } // cylinders
    fclose(disk);
    name[5]++;
  } // units (i.e. disk packs mounted in a drive)
  if(o)close( o );
  fclose(csv);
}

int
main(int ac, char **av){
  pass1();
  return(0);
}
// lpr -P Black1 -o media=legal -o page-left=48 -o cpi=14 -o lpi=8 unload_ckd.c
