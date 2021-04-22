#if 0 // -*- mode:C;coding:utf-8;  -*-
NAME="rebuild-mfd"
  SRC=""
  DST="/usr/local/bin"
  echo gcc -g -m64 -Wall -Werror -Wno-unused-label -o /usr/local/bin/$NAME $NAME.c
  
  gcc -g -m64 -o /usr/local/bin/$NAME $NAME.c

  return 2>/dev/null || exit 0
#endif
  /*
    name now: waits-disk-kits/chickadee
    name nee: /data/Toy.Ten/rebuild-sys-mfd.c
    ============================================================================================ 100
    R E B U I L D     M F D
    KIT
    ============================================================================================ 100
    Scan the Linux file system starting from
    ./KIT/UCFS
    in the current work directory
    and regenerate the [1,1] files named <Project><Programmer>.UFD
    Short prj codes and prg codes are padded with underbar rather than space.

    1. Empty.

    Starting with an empty KIT directory, rebuild will
    make a new 1.1 directory with a single UFD file, which mentions itself.
    Underbars will replace SAIL-WAITS leading space padding.
    KIT/UCFS/                    # reënact SYS:
    KIT/UCFS/1.1/                # directory of directories
    KIT/UCFS/1.1/.__1__1.UFD     # data8 binary
    KIT/UCFS/1.1/__1__1.UFD      #  utf8 text

    2. Idempotent.

    Rebuilding multiple times leaves the KIT/UCFS files unchanged,
    except for all the SAIL WAITS created/written dates inside the *.UFD files
    and the GNU/linux file system dates ( -mtime -ctime -atime ).

    3. Default sets of PPN directories

    skeleton :      1.1     2.1     2.2     2.3     3.1
    small :         BGB.1   REG.1   SYS.*
    medium :
    prog codes : top 200 user_prg_codes + 21 system_prg_codes
    reduced proj codes :
    large :
    extra-large :

    4. Additional so-called Device directories as prj-codes under prg-code 'DEV.'
    Add device named directories:

    KIT/UCFS/DEV.CTY/
    KIT/UCFS/DEV.TTY/

    KIT/UCFS/DEV.LPT/
    KIT/UCFS/DEV.XGP/

    KIT/UCFS/DEV.VDS/
    KIT/UCFS/DEV.DKB/

    KIT/UCFS/DEV.PTR/
    KIT/UCFS/DEV.PTP/
  */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
     
typedef unsigned long long uint64;
typedef          long long  int64;
typedef unsigned long long    u64;
typedef          long long    i64;
typedef unsigned char uchar;

#ifndef PDP10WORD
#define PDP10WORD
typedef union
{
  uint64 fw;
  struct { uint64 c6:6,c5:6,c4:6,c3:6,c2:6,c1:6, pad:28; } sixbit; // ASCII sixbit + 040
  struct {  int64 right:18,  left:18,            pad:28; } half;   // half word left,,right
} pdp10_word;
#endif

// SAIL-WAITS directory <prj><prg>.UFD[1,1] files are composed of
// UFD blocks of four PDP-10 words for each user file
typedef struct UFD { 
  uint64 filnam:36, // six uppercase FILNAM sixbit characters
    pad0:28;
  // high-order 3 bits, because 07777 overflowed on1975-01-04,
  // so 077777 is good until 2052-02-01, I will be 106 years old.
  // then we will use pad1 for seven more bits to exceed Y10K,
  // saildate 17777777 is ISODATE 13239-01-04
  uint64 date_written_high:3,
    creation_date:15, // Right side
    ext:18, // Left side three uppercase .EXT extension
    pad1:28;
  uint64 date_written:12, // Right side
    time_written:11,
    mode:4,
    prot:9, // Left side
    pad2:28;
  uint64 track:36,
    pad3:28;
} UFD_t;

// debug stdout message clutter management
#define bamboo
#define bamoff   0&&

// Global state from pass1 to pass2
UFD_t mfd, ufd;
int ppn_cnt=0;   // count [PRJ,PRG] directories in MFD           __1__1.UFD[1,1]
UFD_t MFD[9999]; // Main File Directory built by pass_one

int file_cnt=0;  // count filnam.ext names within each UFD
int file_cnt_total=0;
UFD_t UFD[9999]; // Main File Directory built by pass_one for each user directory

/*
  The SIXBIT character encoding for the PDP-10 SAIL-WAITS file names
  Six bits ( Modulo 64, base 64 ) represent 64 glyphs as follows:

  26 UPPERCASE letters : ABCDEFGHIJKLMNOPQRSTUVWXYZ
  10 numeric digits    : 0123456789
  28 other characters  : ␣ ! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ ] ^ _

  space, bang, double quote, hash, dollar, percent, ampersand, single quote
  parentheses left and right
  asterisk, plus, comma, minus, period, slash,
  colon, semicolon,
  less than, equals, greater than
  question mark
  at sign
  square brackets left and right
  caret
  underbar
*/
char *sixbit_table= " !\"" "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ" "[\\]^_";
char *sixbit_table_="_!\"" "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ" "[\\]^_";

void
omit_spaces(char *q)
{
  char *t,*p;
  for (p= t= q; *p; p++)
    if (*p != ' ')
      *t++ = *p;
  *t++ = 0;
}
void
space_to_underbar(char *p)
{
  for (;*p;p++)
    if (*p==' ')
      *p = '_';
}
/* ascii SIXBIT decoding functions
 * -------------------------------------------------------------------- *
 * sixbit     word into ASCII string 
 * sixbit halfword into ASCII string
 */
char *
sixbit_u64_into_ascii( char *p0, u64 u, char *tbl )
{
  char *p=p0;
  pdp10_word w;
  w.fw = u;
  // decode SIXBIT into ASCII by arithmetic would simply be + 040
  *p++ = tbl[w.sixbit.c1];
  *p++ = tbl[w.sixbit.c2];
  *p++ = tbl[w.sixbit.c3];
  *p++ = tbl[w.sixbit.c4];
  *p++ = tbl[w.sixbit.c5];
  *p++ = tbl[w.sixbit.c6];
  *p++ = 0;
  return p0;
}

void
set_mfd_filnam_ext( UFD_t *u, char *filnam, char *ext){
  int i;
  bzero(u,sizeof(*u));
  if(filnam)
    for(i=6;i>0;i--){
      char c = filnam[i-1]; // right to left
      char *q = index(sixbit_table,c);
      uint64 qq=0;
      if (q) qq = q - sixbit_table;
      {
        u->filnam >>= 6;             // right six   1x6
        u->filnam |= ( qq << 30);    // left thirty 5x6
        // printf("%2d %c %2lld %03llo %06llo\n",i,c,qq,qq,(uint64)u->filnam);
      }
    }
  if(ext)
    for(i=strlen(ext);i;i--){
      char c = ext[i-1]; // right to left
      char *q = index(sixbit_table,c);
      uint64 qq=0;
      if(q) qq = q - sixbit_table;
      if(qq){
        u->ext >>= 6;                // right six bits   1x6
        u->ext |= ( qq << 12);       // left twelve bits 2x6
        // printf("%2d %c %2lld %03llo %03llo\n",i,c,qq,qq,(uint64)u->ext);
      }
    }
}

void
set_ufd_filnam_ext( UFD_t *u, char *filnam, char *ext){
  int i;
  bzero(u,sizeof(*u));
  if(filnam)
    for(i=strlen(filnam);i;i--){
      char c = filnam[i-1]; // right to left
      char *q = index(sixbit_table,c);
      uint64 qq=0;
      if(q) qq = q - sixbit_table;
      if(qq){
        u->filnam >>= 6;             // right six   1x6
        u->filnam |= ( qq << 30);    // left thirty 5x6
        // printf("%2d %c %2lld %03llo %06llo\n",i,c,qq,qq,(uint64)u->filnam);
      }
    }
  if(ext)
    for(i=strlen(ext);i;i--){
      char c = ext[i-1]; // right to left
      char *q = index(sixbit_table,c);
      uint64 qq=0;
      if(q) qq = q - sixbit_table;
      if(qq){
        u->ext >>= 6;                // right six bits   1x6
        u->ext |= ( qq << 12);       // left twelve bits 2x6
        // printf("%2d %c %2lld %03llo %03llo\n",i,c,qq,qq,(uint64)u->ext);
      }
    }
}

// compare function for qsort, type casting orgy.
static int
compare_ufd_p(const void *v1, const void *v2){
  UFD_t *u1 = (UFD_t *)v1;
  UFD_t *u2 = (UFD_t *)v2;     
  long q = (long)(u1->filnam) - (long)(u2->filnam);  
  return (int)( q<0 ? -1 : q>0 ? 1 : 0 );  
}

void pass_one(){
  FILE *mfd_text; // write text version of the MFD into KIT/UCFS/1.1/__1__1.UFD
  DIR *users, *files;
  struct dirent *user, *file;
  char ppn_path[1024]; // KIT/UCFS/1.1/BGB__1/
  char filepath[1024]; // KIT/UCFS/BGB__1/FILNAM.EXT
  char prog[8], proj[8], filnam[8], ext[8];
  char ppn_ufd[12];  // "BGB__1"
  int i,n;
  //
  users = opendir("KIT/UCFS/");
  // For linux directories in KIT/UCFS/ that match numeric uppercase "PRG.PRJ" appearance and length
  while (users && (user = readdir(users)) )
    {
      char *dir_name = user->d_name;
      if(*dir_name=='.')continue; // Skip dot prefixed names
      if(strlen(dir_name)>7)continue; // skip name is too long
      bzero(prog,sizeof(prog));
      bzero(proj,sizeof(proj));
      n = sscanf( dir_name, "%3[0-9A-Z].%3[0-9A-Z]", prog, proj );
      if(n!=2) continue; // not uppercase numeric
      
      // printf("user %s\n", user->d_name );
      if( n==2 ){
        sprintf( ppn_ufd, "%3.3s%3.3s", prog, proj );
        sprintf( ppn_path, "KIT/UCFS/%s/", user->d_name );
        printf( "ppn_path %s\n",ppn_path);        
        set_mfd_filnam_ext( &MFD[ppn_cnt], ppn_ufd, "UFD" );
        space_to_underbar( ppn_ufd );           
        ppn_cnt++;
        
        files = opendir( ppn_path );
        file_cnt = 0;
        while (files && (file = readdir(files))) // for FILEs in PPN
          {
            char *name = file->d_name;
            //            puts(name);
            if(!strcmp(name,".") || !strcmp(name,".."))continue; // Skip dot and dotdot
            if(strlen(name)>10)continue;        // skip too long
            if( name[0] != '.' )continue;       // skip when NO data8 dot prefix
            bzero(filnam,sizeof(filnam));
            bzero(ext,sizeof(ext));
            n = sscanf( name,".%6[0-9A-Z_].%3[0-9A-Z]", filnam, ext );
            if(n==1 || n==2)
              {
                sprintf( filepath, "KIT/UCFS/%s/%s", user->d_name, file->d_name );
                // printf("filepath %s\n",filepath);
                set_ufd_filnam_ext( &UFD[file_cnt], filnam, ext );
                file_cnt++;
                file_cnt_total++;
              } // filnam ext done
          } // files of one user-directory done
        printf("directory #%d has %6d files\n", ppn_cnt, file_cnt );
        qsort( &UFD[0], file_cnt, sizeof(UFD_t), compare_ufd_p );
        {
          int m;
          char ufd_path[32];
          // sprintf( mfd_path, "KIT/UCFS/1.1/.__1__1.UFD" );
          sprintf( ufd_path, "KIT/UCFS/1.1/.%6.6s.UFD", ppn_ufd );
          printf("ufd_path = %s\n", ufd_path );
          m = open( ufd_path, O_CREAT|O_TRUNC|O_WRONLY,0644 ); 
          write( m, UFD, file_cnt*sizeof(UFD_t));
          if(errno){ fprintf(stderr,"ERROR: write file \"%s\" error %d\n",ufd_path,errno); exit(1); }
          close(m);  
        }
        {
          char ufd_text_path[32];
          sprintf( ufd_text_path, "KIT/UCFS/1.1/%6.6s.UFD", ppn_ufd );
          FILE *ufd_text = fopen( ufd_text_path,"w");
          for(i=0;i<file_cnt;i++){
            u64 z = (u64)UFD[i].filnam;
            char zz[12];
            u64 *w = (u64 *)&UFD[i]; // the UFD is a four word block
       
            zz[0] = sixbit_table_[(z>>30)&077 ];       
            zz[1] = sixbit_table_[(z>>24)&077 ];
            zz[2] = sixbit_table_[(z>>18)&077 ];
            zz[3] = sixbit_table_[(z>>12)&077 ];
            zz[4] = sixbit_table_[(z>> 6)&077 ];
            zz[5] = sixbit_table_[(z    )&077 ];
            zz[6] = 0;
            zz[7] = sixbit_table_[(w[1]>>30)&077 ];
            zz[8] = sixbit_table_[(w[1]>>24)&077 ];
            zz[9] = sixbit_table_[(w[1]>>18)&077 ];
            zz[10]= 0;
       
            printf("%6d %12llo %6.6s.%3.3s[%s,%s] %12llo %12llo %12llo \n",
                   i, z, zz, zz+7, proj,prog, w[1],w[2],w[3] );
            fprintf(ufd_text,"%6d %12llo %6.6s.%3.3s[%s,%s] %12llo %12llo %12llo \n",
                    i, z, zz, zz+7, proj,prog, w[1],w[2],w[3]  );

          }
          fclose(ufd_text);
        }
      } // user ppn done
      
    } // users
  printf("%6d directories\n%6d files\n", ppn_cnt, file_cnt_total );
  qsort( &MFD[0], ppn_cnt, sizeof(UFD_t), compare_ufd_p );

  mfd_text = fopen("KIT/UCFS/1.1/__1__1.UFD","w");
  for(i=0;i<ppn_cnt;i++){
    u64 z = (u64)MFD[i].filnam;
    char zz[8];
    u64 *w = (u64 *)&MFD[i]; // the UFD is a four word block
       
    zz[0] = sixbit_table_[(z>>30)&077 ];       
    zz[1] = sixbit_table_[(z>>24)&077 ];
    zz[2] = sixbit_table_[(z>>18)&077 ];
    zz[3] = sixbit_table_[(z>>12)&077 ];
    zz[4] = sixbit_table_[(z>> 6)&077 ];
    zz[5] = sixbit_table_[(z    )&077 ];
    zz[6] = 0;
       
    printf("%6d %12llo %6.6s.UFD[1,1] %12llo %12llo %12llo \n", i, z, zz, w[1],w[2],w[3] );
    fprintf(mfd_text,"%6d %12llo %6.6s.UFD[1,1] %12llo %12llo %12llo \n", i, z, zz, w[1],w[2],w[3]  );
    omit_spaces( ppn_path );
  }
  fclose(mfd_text);
}

void pass_two(){
  int i;
  for(i=1;i<ppn_cnt;i++){
  }
  // obsolete
#if 0
  {
    printf("%s\n",ep->d_name);
    if((*ep->d_name!='.') && strlen(ep->d_name)<=7)
      {
        sprintf( ppn_path, "KIT/UCFS/%s/", ep->d_name );
        puts( ppn_path );
        dp2 = opendir( ppn_path );
          
        sprintf( ufd_path, "KIT/UCFS/1.1/.%s/", ep->d_name );
        while (dp2 && (ep2 = readdir (dp2)))
          {
            printf("   %s\n",ep->d_name);              
            if(!( !strncmp(ep2->d_name,".",1) || !strncmp(ep2->d_name,"..",2) )){
              printf("%s%s\n",ppn_path,ep2->d_name);
              ext = ep2->d_name;
              filnam = strsep( &ext, "." );
              printf( "filnam='%s' ext='%s'\n", filnam, ext);
                
              set_ufd_filnam_ext(ufd,filnam,ext);
              write(o,&ufd,sizeof(ufd));
            }
          }
        close(o);
        (void)closedir(dp2);
      }
  }
#endif
}

int
main (void)
{
  int i,m,o;
  char mfd_path[32]; // always        KIT/UCFS/1.1/.__1__1.UFD
  char ppn_path[32]; // example       KIT/UCFS/BGB.1/               directory linux
  char ufd_path[32]; // example       KIT/UCFS/1.1/.BGB__1.UFD      directory WAITS for [1,BGB]
  char *filnam,*ext;
  DIR *dp,*dp11;
  DIR *dp2;
  struct dirent *ep,*ep2;
  struct stat statbuf;

  dp = opendir("./KIT/UCFS");
  if(!dp){
    puts(
         "\n"
         "   ./KIT/UCFS link NOT found in your current working directory !\n\n"
         "   Do the 'mkdir -p ./KIT/UCFS' yourself to start a virgin kit\n"
         "   for a SAIL WAITS disk file kit environment - or -\n"
         "   link to an existing FS, Franken-Stein File System, 'ln -s /d/large/ucfs SYS' - or -\n"
         "   cd to the working directory, where you should have been.\n"
         "\n"
         );
  }
  if(stat("KIT/UCFS/1.1",&statbuf) && errno==ENOENT) mkdir("KIT/UCFS/1.1",0777);
  
  // Write brand new MFD __1__1.UFD Master File Directory
  // with an entry for each [ Project , Programmer ] area found under SYS
  dp11 = opendir("KIT/UCFS/1.1");
  
  pass_one();
  
  sprintf( mfd_path, "KIT/UCFS/1.1/.__1__1.UFD" );
  m = open( mfd_path, O_CREAT|O_TRUNC|O_WRONLY,0644 ); 
  write(m,MFD,ppn_cnt*sizeof(UFD_t));
  
  if(errno){ fprintf(stderr,"ERROR: write file \"%s\" error %d\n",mfd_path,errno); return 1; }
  close(m);
  
  return 0;
}
// listing to Black1
// lpr -P Black1 -o media=legal -o page-left=48 -o cpi=14 -o lpi=8 rebuild-sys-mfd.c
 
