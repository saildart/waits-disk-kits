/* track.d
// -*- coding: utf-8; mode: D; -*-
// ------------------------------------------------------------------------------------------------
// Bruce Guenther Baumgart, copyright©2014-2018, license GPLv3.
// ------------------------------------------------------------------------------------------------

This module recreates, for recreation (intentional pun),
the file system format used by the SYSTEM.DMP[J17,SYS] July 1974.
It is based on a close reading (and execution) of the J17 source code files ALLDAT, DSKSER, DSKINT,
as well as the eponymous disk verification utility

        RALPH[ACT,REG]11        sn#117592 1974-08-26 23:16

written by Ralph E. Gorin
and the dump and restore tape utility

        DART[TAP,REG]28         sn#124192 1974-10-03 12:30

also written by Ralph Gorin.

The SAIL disk file system in July 1974 was implemented for an IBM-3330 with four removable disk packs.
By the end of July the double density 200 Megabyte packs had been deployed.
Three of the disk pack positions where used for the public shared file system DSK:
whilst the fourth pack position served either as a private project UDP:, User Disk Pack,
or as the swap device when the Librascope swapping disk was unavailable. In 1974 the device name "SYS:"
was equivalenced to DSK:[1,3] the CUSP executible binary directory. CUSP, Commonly Used System Program.

A brief narrative of the physical IBM disk terminology of 1974 into the local Stanford software vocabulary, 
goes as follows: There are 4 disk packs, each pack has 800. cylinders, each cylinder has 19. tracks.
Each Track consists of 1 'retrieval record' of 32. words followed by 18. 'data records' of 128. words.
Thus one Track with its prefixed retrieval information occupies 2336. PDP10 words and carries
a payload of 2304. PDP10 words of data. Numbers postfixed with a decimal point remind me,
and anyone else who may read this source code, which numbers are decimal.
Octal notation was always the default in assembly language in 1974 on PDP-10 systems.

Another PDP-10 vocabulary trap is the term 'byte' which in DSKSER refers to filling and emptying
a buffer using the variable width ILDB and IDPB opcodes to move 7-bit characters or 36-bit words.
The two token phrase "byte_count" in DSKSER comments is a hazard for a modern code reader.

Track Zero was the SAT, Storage Allocation Table. It carried a 57. word header, followed by 1267. words
for storage allocation track bits, a bit value of 1 indicates that a track is in use.
The final 980. words of track zero were unused.

Track One was the MFD, Master File Directory, which itself was a file named "  1  1.UFD" 
or rather "SYS:↓  1  1↓.UFD[1,1]" for citing on a PDP-10 command line.
All such disk directory files where comprised of four word UFD entries containing

        the FILNAM.EXT filename in a radix-64 ASCII code called SIXBIT,
        its creation date,
        a nine bit protection code,
        a four bit I/O mode, 
        the date time last written and 
        a track number where the file content begins.

The 1974 surviving files preserved by the DART tapes
have around 50,000 distinct file name entries, with 65,000 distinct blob content.
There are more blobs than names because a file's content changes over time, the "version" phenomena.
The population of all the DART archive files available at end-of-year 1974 is 140,000 blobs with 100,000 names;
since the DART tapes started in October 1972 and in addition a number of earlier 
files were brought back online (and then archived) from
others tape formats or via the primitive ARPA net.

In 1974 at Stanford A.I. there was no mechanism for hard linking multiple file names
to one file content track as is done in GNU/Linux file systems.
So, sad to say, redundant content occupied additional disk space for each exact copy.
Also, sad to say, the trade off between variable file size statistics
and the somewhat large fixed block size (fixed track size) leads to a further loss of disk space
to empty padding of underfilled tracks.

An example is the damn Form-Feed-File
which contains a single formfeed character, octal 014;
that is where an eight bit byte of data occupies a full 2336 word track,
10,512 bytes of disk space, and appears 111 times in the archive.
*/

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
/*
 * Define the four word entry for a UFD, User File Directory.
 * UFD are also files, the master file diectory is in track #1
 * and its filename is '  1  1.UFD' it contains further UFD entries
 * foreach [ project , programmer ] pair.
 * For all of SAILDART there are 7698. distinct ppn codes (MFD fits in 14 tracks),
 * for EOY 1974 there were 2129. distinct ppn codes (MFD fits in 4 tracks).
 */
struct UFDent{ 
  mixin(bitfields!(ulong,"filnam",36,ulong,"",28));
  mixin(bitfields!(
                   uint,"creation_date",15,    // Right side
                   uint,"date_written_high",3, // high-order 3 bits, overflowed 4 Jan 1975.
                   ulong,"ext",18,             // Left side
                   uint,"",28));
  mixin(bitfields!(uint,"date_written",12,     // Right side
                   uint,"time_written",11,
                   uint,"mode",4,
                   uint,"prot",9,              // Left side
                   uint,"",28));
  mixin(bitfields!(long,"track",36,
                   uint,"",28));
}

/*
// Disk Track = RIB + DATA
// RIB, the Retrieval Information Block (generic sense of block,
// at variance with the more frequently used "Block" for Track.

The re-enactment DATE, SYSTEM variable THSDAT, is always 26 July 1974
so ((Y-1964)*12+M-1)*31+(D-1) is (120. + 6)*31. + 25. == 3931. == 07533 during re-enactment.
By setting the file reference date to 07533 here 'thsdat' is octal!7533
prevents RIB update disk writes during the re-enactment
since every file is marked as already having been used on 26 July 1974.
        ref_date
        dmp_date
*/
struct RIB{
  // RIB 32.PDP10 words prefixed to 2304. words of data. So RIB + DATA = TRACK, 32. + 2304. = 2336.
  // Renamed             Ralph name
  // ----------------    ----------------------------------------------------------------
  WORD_PDP10 filnam,     // DDNAM sixbit
    ext,                 // DDEXT sixbit
    prot,                // DDPRO        whatever
    ppn,                 // DDPPN sixbit
    location,            // DDLOC as track# (somewhat like a disk block#)
    filesize,            // DDLNG in PDP-10 words !
    ref_datetime,        // DREFTM
    dmp_datetime,        // DDMPTM
    firstgroup,          // DGRP1R init value 1 until filesize exceeds
    nextgroup,           // DNTXGP init value 0
    satid;               // DSATID init value 0 in core SAT table
  WORD_PDP10[5] dqinfo;  // DQINFO special information (ie passwords)
  WORD_PDP10[16] dptr;   // DPTR   32. Track (aka Block) numbers for this group,
                         // which are packed left then right, BIG-endian PDP-byte-order.
} // ----------------    ----------------------------------------------------------------
struct TRACK{
  union {
    SATHEAD sat_head; // first 32 words of SAT block
    RIB rib;          // track retrieval information
  }
  union {  // payload
    SAT sat_data; // The storage allocation bit table for the full file system
    UFDent[576] ufdent;        // 576. UFD user file directory entries
    WORD_PDP10[2304] data;     // file content data
    void[2304*8] blob;         // file content data as blob of bytes
  }
}
// TRACK track[45600]; // 3 packs × 800. cylinders × 19 tracks per cylinder
