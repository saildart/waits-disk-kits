#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;

/* Header block */
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
int last = 0;
int track = 0;

unsigned long long int
get_word() {
    char  buffer[100];
    unsigned long long int wd;
    char  *p;
    
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
       last = 1;
       return 0;
    }

    words++;
    wd = 0;
    for (p = buffer; *p != '\0'; p++) {
        if (*p < '0' || *p > '7')
           break;
        wd = (wd << 3) | (*p - '0');
    }
    return wd;
}
    

int
main(int argc, char *argv[]) {
    unsigned long long int wd;
    char  name[40];
    char  *cbuf;
    int   tsize;
    struct pmp_header hdr;
    FILE  *disk;
    int   cyl, hd, rec, pos, i, sector;

    memset(&hdr, 0, sizeof(struct pmp_header));
    memcpy(&hdr.devid[0], "CKD_P370", 8);
    hdr.heads = 19;
    hdr.tracksize = (13165 | 0x1ff) + 1;
    hdr.devtype = 0x30;
    hdr.highcyl = 815;
    tsize = hdr.tracksize * hdr.heads;
    if ((cbuf = (uint8 *)calloc(tsize, sizeof(uint8))) == 0)
         return 1;
    strcpy(name, "SYS000.ckd");
    while (!last) {
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
                for (i = 0; i < 16; i++) {
                    wd = get_word(); 
//fprintf (stdout, "c:%3d h:%2d s: %2d w:%5d pos %012llo\n", cyl, hd, rec-1, pos, wd);
                    cbuf[pos++] = (wd >> 28) & 0xff;
                    cbuf[pos++] = (wd >> 20) & 0xff;
                    cbuf[pos++] = (wd >> 12) & 0xff;
                    cbuf[pos++] = (wd >> 4) & 0xff;
                    cbuf[pos] = (wd & 0xf) << 4;
                    wd = get_word(); 
//fprintf (stdout, "c:%3d h:%2d s: %2d w:%5d pos %012llo\n", cyl, hd, rec-1, pos, wd);
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
//fprintf (stdout, "c:%3d h:%2d s: %2d w:%5d pos %012llo\n", cyl, hd, rec-1, pos, wd);
                        cbuf[pos++] = (wd >> 28) & 0xff;
                        cbuf[pos++] = (wd >> 20) & 0xff;
                        cbuf[pos++] = (wd >> 12) & 0xff;
                        cbuf[pos++] = (wd >> 4) & 0xff;
                        cbuf[pos] = (wd & 0xf) << 4;
                        wd = get_word(); 
//fprintf (stdout, "c:%3d h:%2d s: %2d w:%5d pos %012llo\n", cyl, hd, rec-1, pos, wd);
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
         fclose(disk);
         name[5]++;
     }
fprintf(stderr, "Words: %d\n", words);
}
