#if 0 // -*- mode:C; coding:utf-8; -*-
NAME="DASD_into_chickadee"
  full_path=$(realpath $0)
  echo FROM $full_path
  SRC=$(dirname $full_path)
  DST="/usr/local/bin"
  #           -Wall
  gcc -g -m64  -Wall     -Werror -o $DST/$NAME  $SRC/$NAME.c && echo -n OK || echo -n FAILED
  echo " $DST/$NAME"
  return 2>/dev/null || exit 0
#endif
  /*
    program name now: DASD_into_chickadee   
        associated with chickadee_into_UCFS
    
    original name nee: load_ckd.c
    From Richard Cornwell via           http://sky-visions.com/dec/waits/

    See nomensclature rationale in the README.md
    MIT License (I shall assume) `2021-04-23 bgbaumgart@mac.com'

    There is no makefile, just execute this source file.
  */  
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

  typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;

// Header block
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

int words = 0;
int empty = 0;
int last = 0;
int track = 0;

int fd=0;

unsigned long long int
get_word() {
  unsigned long long int wrd=0;
  if(last){
    empty++;
    return 0;
  }
  if( read(fd,&wrd,8) != 8 ){
    last = 1;
  }
  words++;
  return wrd;
}    

int
main(int ac, char *av[]) {
  unsigned long long int wd;
  char  name[256];
  char  *cbuf;
  int   tsize;
  struct pmp_header hdr;
  FILE  *disk;
  int pack, cyl, hd, rec, pos, i, sector;
  int packcount=0;
  char *kitpath;
  if(ac < 2){
    fprintf(stderr,"\nsynopsis: %s kitpath\n\n",av[0]);
    return 1;
  }
  kitpath = av[1];
  sprintf( name, "%s%s", kitpath, "/DASD.data8" );
  fd = open( name, O_RDONLY );
  if(fd<0){
    fprintf(stderr,
            "\n\t ERROR: input file ./DASD.data8 NOT found.\n\n"
            "\t Your current-working directory must be set to\n"
            "\t the WAITS DISK KIT you wish to re-build\n"
            "\t and it must have a disk-image DASD.data8 file.\n\n"
            "\t Suggestion: \"cd $KIT\" change environment as needed.\n\n"
            );
    exit(1);
  }
  if(ac==2) packcount=atoi(av[1]);
  memset(&hdr, 0, sizeof(struct pmp_header));
  memcpy(&hdr.devid[0], "CKD_P370", 8);
  hdr.heads = 19;
  hdr.tracksize = (13165 | 0x1ff) + 1;
  hdr.devtype = 0x30;
  hdr.highcyl = 815;
  tsize = hdr.tracksize * hdr.heads;
  if ((cbuf = (char *)calloc(tsize, sizeof(char))) == 0)
    return 1;
  sprintf( name, "%s%s", kitpath, "/0.ckd");
  for (pack = 0; (packcount==0 && !last) || ( pack < packcount ) ; pack++) {
    disk = fopen(name, "w");
    fprintf(stderr, "%s: ", name);
    fseek(disk, 0, SEEK_SET);
    fwrite(&hdr, 1, sizeof(struct pmp_header), disk);
    for (cyl = 0; cyl < 800; cyl++) {
      pos = 0;
      for (hd = 0; hd < 19; hd++) {
        int cpos = pos;
        rec = 0;
        //fprintf(stdout, "Track %d %d %d\n", track++, cyl, hd);
        cbuf[pos++] = 0;            /* HA */
        cbuf[pos++] = (cyl >> 8);
        cbuf[pos++] = (cyl & 0xff);
        cbuf[pos++] = (hd >> 8);
        cbuf[pos++] = (hd & 0xff);
        cbuf[pos++] = (cyl >> 8);   /* R1 Rib block */
        cbuf[pos++] = (cyl & 0xff);
        cbuf[pos++] = (hd >> 8);
        cbuf[pos++] = (hd & 0xff);
        cbuf[pos++] = rec++;          /* Rec */
        cbuf[pos++] = 0;              /* keylen */
        cbuf[pos++] = 0;              /* dlen */
        cbuf[pos++] = 144;            /*  */
        // fprintf(stdout, "Disk %s Track x%06d cyl#%3d head#%2d %8d\n", name, track++, cyl, hd, pos);
        for (i = 0; i < 16; i++) {
          wd = get_word();
          cbuf[pos++] = (wd >> 28) & 0xff;
          cbuf[pos++] = (wd >> 20) & 0xff;
          cbuf[pos++] = (wd >> 12) & 0xff;
          cbuf[pos++] = (wd >> 4) & 0xff;
          cbuf[pos] = (wd & 0xf) << 4;
          wd = get_word();
          cbuf[pos++] |= (wd & 0xf);
          cbuf[pos++] = (wd >> 28) & 0xff;
          cbuf[pos++] = (wd >> 20) & 0xff;
          cbuf[pos++] = (wd >> 12) & 0xff;
          cbuf[pos++] = (wd >> 4) & 0xff;
        }
        for (sector = 0; sector < 18; sector++) {
          cbuf[pos++] = (cyl >> 8);   /* R1 */
          cbuf[pos++] = (cyl & 0xff);
          cbuf[pos++] = (hd >> 8);
          cbuf[pos++] = (hd & 0xff);
          cbuf[pos++] = rec++;        /* Rec */
          cbuf[pos++] = 0;            /* keylen */
          cbuf[pos++] = 2;            /* dlen = 576 */
          cbuf[pos++] = 0100;         /*  */
          for (i = 0; i < 64; i++) {
            wd = get_word();
            cbuf[pos++] = (wd >> 28) & 0xff;
            cbuf[pos++] = (wd >> 20) & 0xff;
            cbuf[pos++] = (wd >> 12) & 0xff;
            cbuf[pos++] = (wd >> 4) & 0xff;
            cbuf[pos] = (wd & 0xf) << 4;
            wd = get_word();
            cbuf[pos++] |= (wd & 0xf);
            cbuf[pos++] = (wd >> 28) & 0xff;
            cbuf[pos++] = (wd >> 20) & 0xff;
            cbuf[pos++] = (wd >> 12) & 0xff;
            cbuf[pos++] = (wd >> 4) & 0xff;
          }
        }
        cbuf[pos++] = 0xff;           /* End record */
        cbuf[pos++] = 0xff;
        cbuf[pos++] = 0xff;
        cbuf[pos++] = 0xff;
        if ((pos - cpos) > tsize) {
          fprintf(stderr, "Overfull %d %d\n", pos-cpos, tsize);
        }
        pos = cpos + hdr.tracksize;
      }
      fwrite(cbuf, 1, tsize, disk);
      if ((cyl % 10) == 0)
        fputc('.', stderr);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Words: %8d  Empty: %8d   %4.1f %% full.\n",
            words, empty, words*100./(float)(words+empty));
    words=0;
    fclose(disk);
    name[strlen(name)-5]++;
  }
}
