// Fetch URL: https://github.com/saildart/waits-disk.git -*- coding: utf-8; mode: D; -*-
// sub-directory waits-disk/ralf
// -------------------------------------------------------------------------------------------------
// Bruce Guenther Baumgart, copyright©2014-2018,2021 license GPLv3.
// ------------------------------------------------------------------------------------------------- 
//                              R       A       L       F
// Max ------------------------------------------------------------------------------------------- line length
// Ruler 1        2         3         4         5         6         7         8         9        10        110
// 456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.
// -----------------------------------------------------------------------------------------------------------
/*
  This program is named "RALF" in honor of Ralph E. Gorin,
  who named the Waits PDP-10 file system verification program, RALPH, after himself.
  A close reading of the RALPH.fai source code has enabled writing this RALF which re-constructs
  disk images of a file system suitable for the 1974 hardware emulators.
  ¶
  Here we build a PDP-10 SU-AI-Lab system disk image for
  the concatenation of a smallish number of 200 Megabyte IBM-3300 disk packs.
  One, two, or three packs is reasonable;
  up to one hundred disk packs is possible to have twenty Gigabytes online
  which is approximately the total offline capacity of the SAILDART tape archive.
  The disk-image () suitable for the 1974 hardware re-enactments;
  but it is not directly usable on actual museum grade hardware without
  a further format conversion (IBM CKD / Merlin and Chickadee).
  The disk-image file-set is specified by a database CSV formated table (filename,sn,…) rows
  The actual PDP-10 binary data is copied from (SAILDART digital curator accessible)
  unix file system mounted at pathname /data8/sn/
  where each serial numbered blob is available in DATA8 format, binary 8-bytes per PDP-10 word (zero-left-padded)
  from 000001 to something like 886781;
  or more recently the top serial number is 888474 for authentic blobs
  as more damaged file blobs have become available from the DART archive.

  To verify the highest available blob serial number, sn,
  execute the bash string "(cd /data8/sn;readdir .|sort|tail -1)".
  Blobs beyond sn# 888888 (the Hong-Kong-Lucky serial-number) are not historically authentic,
  but have been fabricated to augment the simulated environment slightly as a stepping stone
  towards handling large quantities of synthetic files (neo-WAITS) for "Look-and-Feel" reënactment.
  ¶
        track = new TRACK[45600];   // 3 packs × 800. cylinders × 19 tracks per cylinder is 458 Megabytes
        track = new TRACK[131072];  // example 17-bit track-address space 2.3 Gigabytes
        track = new TRACK[262144];  // max out 18-bit track-address space 4.6 Gigabytes
  ¶
  NOTE: The System J17 Directory Entry LOC Location Track-Address Space is 30-bits internally,
  since it supports a generous 6-bit low order sector address.
  If a huge DASD for the Re-Enactment is required,
  30-bit or even 36-bit Track addresses could be implemented.
  ¶
  Also we may implement file content indirection, with negative -sn for file-entry LOC location,
  so the initial track space needs to only hold the UFD directory data,
  and blobs are loaded into tracks (or into a track cache) at simulation time.
  ● expect fewer than 2000 programmer codes
  ● expect fewer than 8000 ppn codes
  ● expect fewer than 1 million data blobs, unique sn#
  ● expect fewer than 2 million filename_ppn, would needs 500000 PDP10 words (=217 full tracks)
  ● tolerate 32000 ppn codes in order to handle versions for the 20 year SAILDART epoch (=240 months).
  */
import std.stdio;
import std.conv;
import std.file;
import std.math;
import std.algorithm;
import std.bitmanip;
import std.string;
import core.bitop;
mixin(import("word.d"));
mixin(import("track.d"));

//
// GLOBAL state for this module
//
TRACK[]         track;
char[][string]   ufd_filename;          // keyed on PPN
char[][]         ufd_filnam_swapped;    // so we can sort strings to get PRG,,PRJ order
int[string]      ufd_filecount;         // number of actual files within each UFD
UFDent[][string] ufd_filecontent;       // The content of each UFD file is an array of UFDent four word slots.
//
// PDP10 file system constants
//
immutable ulong sixbit_ufd = octal!654644; // sixbit/UFD/
immutable ulong sixbit_1_1 = octal!21000021; // sixbit/  1  1/
immutable ulong sixbit_1_2 = octal!21000022; // sixbit/  1  2/
immutable ulong sixbit_1_3 = octal!21000023; // sixbit/  1  3/
//
// For this Re-Enactment, TODAY's date is 26 July 1974
// expressed as ((Y-1964)*12+(M-1))*31+(D-1) = octal 07533 = 3931.
// and the latter-day high-order bits are still sleeping at ZERO.
int thsdat = octal!7533;
// Note: The old 12-bit date field overflowed on 4 January 1975,
// the higher order extra bits were implemented for DART and RALPH, just-in-time for '75.
int ufd_tracks_used=0;
int data_track=1000; // data content
int free_track=1000; // start allocating regular files from here
auto MASK=  [0x000000000,
             0x000000001,0x000000003,0x000000007,0x00000000F,
             0x00000001F,0x00000003F,0x00000007F,0x0000000FF,
             0x0000001FF,0x0000003FF,0x0000007FF,0x000000FFF,
             0x000001FFF,0x000003FFF,0x000007FFF,0x00000FFFF,
             0x00001FFFF,0x00003FFFF,0x00007FFFF,0x0000FFFFF,
             0x0001FFFFF,0x0003FFFFF,0x0007FFFFF,0x000FFFFFF,
             0x001FFFFFF,0x003FFFFFF,0x007FFFFFF,0x00FFFFFFF,
             0x01FFFFFFF,0x03FFFFFFF,0x07FFFFFFF,0x0FFFFFFFF,
             0x1FFFFFFFF,0x3FFFFFFFF,0x7FFFFFFFF,0xFFFFFFFFF,];

/* Write the blob content of a SAILDART PDP-10 file into tracks
   ==================================================================================
   Place the content of a SAILDART file into DISK tracks.
   Read 8-byte-per-word file content

        from when sn ≠ 0
                data8/sn/{SN} files,
        or when sn == 0
                ./KIT/{programmer.project}/.{filename.extension}

   Copy track sized segments of sn blob into the track data area
   with attention to a RIB retrieval block for each track,
   with up to 32 tracks per group(cluster).
   ¶
   The redundant TRACK file retrieval information was necessary because seek position
   verification was an Operating System level responsibility in 1974.
   There was NO such thing as drive level firmware doing the seek verification.
*/
ulong fetch_file_content(ulong ppn,char[]sn,uint wrdcnt,UFDent[]slot,string pathname,char[]data8_path){
  auto r = ( wrdcnt % 2304 )*8; // Remainder. Number of bytes in last track data.
  auto n = ( wrdcnt / 2304 ) + (( wrdcnt % 2304 ) ? 1 : 0); // tracks needed for this file
  auto a = free_track;
  void[] blob;
  //
  auto verbose = 1; // (wrdcnt == 445824); // multi-group large-file example is at PAE1E.SND[SND,JAM]
  free_track += n;
  if(1)writefln("File %s from %s sn=%s   %6d words  %6d tracks needed   a=%d free_track=%d",
                pathname,data8_path,sn,wrdcnt,n,a,free_track);
  //
  auto m = 2304*8;      // max blob size in bytes per track data area
  try {
    blob = read( data8_path );
  }
  catch (Throwable){
    stderr.writefln("read file FAILED "~"/data8/sn/"~sn);
    return 0;
  }
  auto y = blob.length; // blob size from reading the archive file
  auto z = wrdcnt*8;    // blob size in bytes from wrdcnt
  if(y!=z){
    writefln("wrdcnt=%d does NOT match filesize=%d  " ~ "File sn=%s   %6d words   %s", z,y,sn,wrdcnt,pathname );
    return 0; // track#0 NO CONTENT created for this sn#
  }
  //
  // Place segments of the content blob into N tracks.
  //
  if(n==1){
    auto top = wrdcnt*8;
    track[a].blob[0..top] = blob[0..top];
  }else{
    for(auto i=0;i<n;i++){
      auto offset = i*m;
      auto top = (i==(n-1)) ? r : m; // (sheesh! failed when wrdcnt==2304 exact)
      track[a+i].blob[0..top] = blob[offset..(offset+top)];
    }
  }
  // Track whacking:
  // Whack the file track numbers into the ribs modulo the 32. track group(cluster) size.
  auto rib = new RIB[1]; // Prime Rib, "one rib to bind them all" and the into groups(clusters) of 32.
  // RIB bind track content to the "named files" (at variance to the unix file system model).
  rib[0].filnam.fw = slot[0].filnam;    // File Name !
  rib[0].ext.fw = slot[0].ext<<18;
  rib[0].ppn.fw = ppn;
  rib[0].location.fw = a;               // first track of this file
  rib[0].filesize.fw = wrdcnt;          // length of the file in PDP10 words
  // GROUP groping
  auto group_cnt = 1 + (n/32); // number of groups needed for this file
  // For each cluster #j (most files were small and would fit in one cluster)
  for(auto j=0; j < group_cnt; j++){
    // First group, place 1 in the DGRP1R position of the RIB,
    // then 577 second group, 1153 third group, 1729, 2305, 2881, 3457, etc adding 32*18 each time = +576
    // Tracks (here in the 21st century) are allocated sequentially to the file blob
    //  18. data records per track
    // 128. PDP-10 words per record
    rib[0].firstgroup.fw = ( j==0 )           ? 1 : (1+j*32*18); // First record# of this group
    rib[0].nextgroup.fw  = ((j+1)==group_cnt) ? 0 : (a+j*32+32); // Track# of next group
    // For each track of this file, place its track# into the group DPTR array of the prime RIB (intentional pun).
    for (auto i=0; i < (((j+1)==group_cnt) ? (n%32) : 32); i++ )
      {
        if((i&1)==0){                    // Big End first
          rib[0].dptr[i/2].l = a+j*32+i; // EVEN left half word
        }else{
          rib[0].dptr[i/2].r = a+j*32+i; // ODD right half word
        }
        if(0) // verbose?
          writefln("   %3d  j=%d i=%2d  track=%6d.  dptr[%2d]=%012o",(j*32+i),j,i,a,i/2,  rib[0].dptr[i/2].fw );
      }
    // copy prime RIB into each track of this group(cluster)
    for(auto i=0;i<32;i++){
      track[a+j*32+i].rib = rib[0];
    }
    // clear RIB buffer
    for(auto i=0;i<16;i++){
      rib[0].dptr[i].fw = 0;
    }
  }
  return a; // return first track# of the file's content.
}

// Store UFD into tracks
// ======================================================================================= //
// Place content (the array of file slots) for UFD file 'PrjPrg.UFD[1,1]'
// into its allocated track(s) starting at t0
void store_directory_file( UFDent[]dir, ulong t0, ulong sixbit_ppn){
  auto n = dir.length;
  auto nt= 1 + n/576;
  // copy directory entrys into track data area
  foreach(i,u;dir){
    track[t0 + i/576].ufdent[i%576] = u;
  }
  // Initialize the Prime RIB for group-of-tracks
  auto q=track[t0].rib; // RIB buffer (NOT a pointer). In 'D' we avoid pointers.
  q.filnam.fw        = sixbit_ppn;
  q.ext.fw           = sixbit_ufd << 18;
  q.ppn.fw           = sixbit_1_1;           // This file is PRJPRG.UFD[1,1]
  q.location.fw      = t0;                   // First track# of this PPN.UFD file
  q.filesize.fw      = n*4;                  // four PDP10 word per UFD entry
  q.ref_datetime.fw  = thsdat; // Always 26 July 1974 prevent RIB update during re-enactment.
  q.dmp_datetime.fw  = thsdat;
  q.firstgroup.fw    = 1;
  q.nextgroup.fw     = 0; // All UFD must fit in one group 32.*576. is maximum 18432. files per PPN.
  q.satid.fw         = 0;
  //  WORD_PDP10 dqinfo[5]; // special information
  // place link to each track of the group
  // each RIB points at up to 32. other TRACKS of the GROUP.
  foreach(j;0..nt){ // track offsets of the FIRST group of a file
    auto ty= t0+j;
    switch(j){
    case  0: q.dptr[0].l  = cast(int)ty;break; case  1: q.dptr[0].r  = cast(int)ty;break;
    case  2: q.dptr[1].l  = cast(int)ty;break; case  3: q.dptr[1].r  = cast(int)ty;break;
    case  4: q.dptr[2].l  = cast(int)ty;break; case  5: q.dptr[2].r  = cast(int)ty;break;
    case  6: q.dptr[3].l  = cast(int)ty;break; case  7: q.dptr[3].r  = cast(int)ty;break;
    case  8: q.dptr[4].l  = cast(int)ty;break; case  9: q.dptr[4].r  = cast(int)ty;break;
    case 10: q.dptr[5].l  = cast(int)ty;break; case 11: q.dptr[5].r  = cast(int)ty;break;
    case 12: q.dptr[6].l  = cast(int)ty;break; case 13: q.dptr[6].r  = cast(int)ty;break;
    case 14: q.dptr[7].l  = cast(int)ty;break; case 15: q.dptr[7].r  = cast(int)ty;break;
    case 16: q.dptr[8].l  = cast(int)ty;break; case 17: q.dptr[8].r  = cast(int)ty;break;
    case 18: q.dptr[9].l  = cast(int)ty;break; case 19: q.dptr[9].r  = cast(int)ty;break;
    case 20: q.dptr[10].l = cast(int)ty;break; case 21: q.dptr[10].r = cast(int)ty;break;
    case 22: q.dptr[11].l = cast(int)ty;break; case 23: q.dptr[11].r = cast(int)ty;break;
    case 24: q.dptr[12].l = cast(int)ty;break; case 25: q.dptr[12].r = cast(int)ty;break;
    case 26: q.dptr[13].l = cast(int)ty;break; case 27: q.dptr[13].r = cast(int)ty;break;
    case 28: q.dptr[14].l = cast(int)ty;break; case 29: q.dptr[14].r = cast(int)ty;break;
    case 30: q.dptr[15].l = cast(int)ty;break; case 31: q.dptr[15].r = cast(int)ty;break;
    default:assert(0,"BAD track offset while packing RIB pointers for a .UFD file");
    }
  }
  foreach(t;0..nt){
    track[t0+t].rib=q; // Write the prime RIB into each TRACK
  }
}

ulong swaphalves(ulong w){
  return (w>>18)|((w&0x3FFFF)<<18);
}
ulong sixbit(char[]str){
  WORD_PDP10 w;
  auto q = str.toUpper(); // Avoid Petit-bit-swapping-hack for lower case into SIXBIT.
  switch( q.length ){
  default: // When argument length is greater than six, Truncate to the first six characters.
  case 6:  w.sixbit6 = cast(uint)q[5] - 0x20; goto case 5;
  case 5:  w.sixbit5 = cast(uint)q[4] - 0x20; goto case 4;
  case 4:  w.sixbit4 = cast(uint)q[3] - 0x20; goto case 3;
  case 3:  w.sixbit3 = cast(uint)q[2] - 0x20; goto case 2;
  case 2:  w.sixbit2 = cast(uint)q[1] - 0x20; goto case 1;
  case 1:  w.sixbit1 = cast(uint)q[0] - 0x20; break;
  case 0: break;
  }
  return w.fw;
}
// Set the storage allocation bits to be marked as in use, USED bits, into SAT table.
// for a span of tracks from t1 to (t2-1) inclusive, that is t2 is +1 beyond the span, 
// consistent with the 'D' language slice notation.
void setsatbits(ulong t1, ulong t2){
  ulong q1,r1,q2,r2;
  stdout.writefln("setsatbits(%d,%d)",t1,t2);
  if( t1==t2 || 45599<t1 ){
    return; // do nothing case
  }
  if(t2>45599){ // Over-The-TOP is OK for Read-Only (Farb-ricated) tracks.
    t2 = 45599; // SYSTEM.DMP[J17,SYS] SAT table last bit is forever LSTBTB/ 45,599.
  }
  assert( t2 > t1, "sheesh BAD setsatbits interval");
  q1 = t1/36; r1 = t1%36;
  q2 = t2/36; r2 = t2%36;
  writeln("setsatbits t1=",t1," q1=",q1," r1=",r1,"    t2=",t2," q2=",q2," r2=",r2);
  if(q1==q2){                                           // set the bits in the middle of a word.
    track[0].sat_data.satbit[q1] |= ( MASK[36-r1] & (MASK[r2]<<(36-r2)) );
  }else{
    track[0].sat_data.satbit[q1] |= MASK[36-r1];        // turn on right side of first word.
    track[0].sat_data.satbit[q2] |= MASK[r2]<<(36-r2);  // turn on left side of last word.
  }
  // mid range solid 36 bits per word
  for(auto i=q1+1;i<q2;i++){
    track[0].sat_data.satbit[i] = 0xFFFFFFFFF; // all allocation bits on in mid span.
  }
}

// https://github.com/saildart/waits-disk ralf/main.d
// Max ------------------------------------------------------------------------------------------- line length
int main()
{
  stdout.writefln("\n=== BEGIN ===");
  //  track = new TRACK[91200]; // Lets say TWICE as many as the 1974 hardware had.
  // track = new TRACK[131072]; // 17-bit 2.3 Gigabytes (huge-old-disk-image fits in current-memory)
  // track = new TRACK[262144]; // 18-bit 4.6 Gigabytes (huge-old-disk-image fits in current-memory)
  track = new TRACK[220144];
  stderr.writefln("Maximum number of tracks = %d each track is %d bytes", track.length, TRACK.sizeof );
  double Megabytes = (to!float(track.length) * TRACK.sizeof) / pow(2,20);
  double Gigabytes = (to!float(track.length) * TRACK.sizeof) / pow(2,30);
  stderr.writefln("Maximum DASD size = %8.3f Megabytes = %8.3f Gigabytes", Megabytes, Gigabytes );
  // ====================================================================================== //
  // MFD
  //    The first file is the Master File Directory named '  1  1.UFD'.
  //    Its content starts in track#1, track#0 is for the SAT table.
  //    Its directory entry (for itself as a file) is in slot#0 of track#1.
  //
  auto mfd_filename = "  1  1";
  auto ufd_filename = "  1  1";
  auto ufd_filename_swapped = ufd_filename; // same "  1  1" in this case.
  ufd_filnam_swapped = new char[][1024];
  ufd_filnam_swapped[0] = cast(char[])ufd_filename;
  ufd_filecount[ufd_filename]++;
  int ufdcnt=1; // count +1 the MFD itself is a UFD, so bump the count.
  // ==============================================
  // Read SAILDART metadata for files selected from the DART tapes archive
  // to go into this disk image. Note that there are extra FAKE(Farb) project codes
  // to permit exploring many versions of content for a given filename over the years.
  // For example FAIL.DMP[1,3] has 67 versions found in FAIL.DMP[001,3] up to FAIL.DMP[067,3]
  // or by month code FAIL.DMP[72A,3] or FAIL.DMP[834,3]
  // for accessing the October 1972 or the April 1983 version of FAIL.DMP[1,3]
  //
  // AND multi-USER PRG codes are serial numbered. That is when more than one human was assigned
  // the same PRG code over the years, all such people are assigned numeric PRG codes
  // which (except for 1,2,3 and 100) went unused at Stanford.
  //
  stdout.writeln("\n===  Read ralf.csv ===");
  auto infile = File("./KIT/ralf.csv","r");
  int file_count;
  int val;string nam,ln;
  char[]project,programmer,filename,extension,sn;
  uint year,month,day,hour,minute,second,wrdcnt;

  // Build the two level SAIL directory structure
  // Place UFD directory-entries into 'D' hash arrays
  // or with indirect place-holding DATA Track pointer values -sn
  while( infile.readf("%s,%s,%s,%s,%s,%d,%d-%d-%d %d:%d:%d\n",
                      &programmer, &project,
                      &filename, &extension,
                      &sn,
                      &wrdcnt,
                      &year,&month,&day,
                      &hour,&minute,&second) )
    {
      file_count++;
      // stderr.writef("%5d ",file_count);
      ufd_filename_swapped = format("%3s%3s",programmer,project);
      auto ppn = sixbit(cast(char[])format("%3s%3s",project,programmer));
      auto pathname = format("%-6s.%3s[%3s,%3s]",filename,extension,project,programmer);
      auto data8_path= sn ? "/data8/sn/"~sn :
        "./KIT/UCFS/." ~ programmer ~"."~ project ~"/"~ filename ~ ( extension ? "."~ extension : "" );
      ufd_filecount[ufd_filename_swapped]++;
      //
      // Build four word UFD entry for this filename PPN
      //
      auto slot = new UFDent[1];
      slot[0].filnam = sixbit(filename);
      slot[0].ext    = sixbit(extension)>>18;
      auto saildate = ((year-1964)*12 + month - 1)*31 + day - 1;
      auto sailtime = hour*60 + minute;
      slot[0].creation_date =      saildate; // here CREATION date is same as WRITE date
      slot[0].date_written  =     (saildate % 4096); // low order date 12-bits
      slot[0].date_written_high = (saildate >> 15);  // high order      3-bits
      slot[0].time_written = hour*60 + minute;
      // Pure WRITE all BLOBS into TRACKS !! uncomment the indirection when implemention needed.
      switch(ppn){
      case sixbit_1_2:
      case sixbit_1_3:
        slot[0].track = fetch_file_content(ppn,sn,wrdcnt,slot,pathname,data8_path);
        break;
      default:
        slot[0].track = fetch_file_content(ppn,sn,wrdcnt,slot,pathname,data8_path);
        // slot[0].track = -(to!long(sn)); // Indirection to provide access to large file set
        break;
      }
      //
      // First file for this PPN ?
      // Append PPN slot into the MFD Master_File_Directory which is named "  1  1.UFD"
      //
      if(1==ufd_filecount[ufd_filename_swapped]){
        ufd_filnam_swapped[ufdcnt++] = cast(char[])ufd_filename_swapped;
        ufd_filecontent[ufd_filename_swapped] = new UFDent[0];
      }
      if(0)stderr.writefln("%s   sn# %s  %8d words  %s",pathname,sn,wrdcnt,ufd_filename);
      //
      // Append file slot into its UFD User_File_Directory
      //
      ufd_filecontent[ufd_filename_swapped] ~= slot;
      if(0)stderr.writefln("%s %d",ufd_filename_swapped, ufd_filecontent[ufd_filename_swapped].length );
    }
  stdout.writefln("There are %6d UFD files and %6d ordinary files",ufdcnt,file_count);
  infile.close;

  //
  // Initialize Track#1 RIB Retrieval Information Block
  //
  track[1].rib.filnam.fw                = sixbit(cast(char[])mfd_filename); // sixbit/  1  1/ is 000021000021
  track[1].rib.ext.fw                   = sixbit_ufd<<18; // sixbit/UFD/ is 654644
  track[1].rib.prot.fw                  = 0 ;
  track[1].rib.ppn                      = track[1].rib.filnam; // special case for [1,1]
  track[1].rib.location.fw              = 1;
  track[1].rib.filesize.fw              = 4 * ufdcnt;   // MFD file size in words
  track[1].rib.ref_datetime.fw          = thsdat;
  track[1].rib.dmp_datetime.fw          = thsdat;
  track[1].rib.firstgroup.fw            = 1;
  track[1].rib.nextgroup.fw             = 0;
  track[1].rib.satid.fw                 = 0;
  track[1].rib.dptr[0].l = 1; // this track is the only one in its group

  /*
    Allocate space and write UFD files into tracks,
    while accumulating the directory content for the Master File Directory,
    There are four words for each UFD directory [prj,prg] including the [1,1] master UFD.
    Write final version of MFD into its tracks starting at track#1.
  */
  sort( ufd_filnam_swapped[0..ufdcnt] );
  ufd_filecount[mfd_filename] = ufdcnt;
  auto mfd = new UFDent[ufdcnt];
  ufd_filecontent[mfd_filename] = mfd;
  //
  stdout.writefln("\n=== linefeed ===");
  foreach( i,u; ufd_filnam_swapped[0..ufdcnt]){
    auto tracks_needed = 1 + (ufd_filecount[u]-1) / 576;
    auto sixbit_ppn = swaphalves(sixbit(u));  // XWD project,,programmer sixbit encoded.
    mfd[i].filnam = sixbit_ppn; // Filename is PrjPrg in sixbit
    mfd[i].ext    = sixbit_ufd; // Extension .UFD into 18-bit field
    //    track[1].ufdent[i].creation_date
    //    track[1].ufdent[i].prot
    //    track[1].ufdent[i].mode
    //    track[1].ufdent[i].time_written
    //    track[1].ufdent[i].date_written
    // ALLOCATE track space for UFD file.
    // Set first track of this UFD into its MFD entry.
    mfd[i].track = 1 + ufd_tracks_used;
    ufd_tracks_used += tracks_needed;
    // verbose debug progress message
    if(1)stdout.writefln("file#%d filename='%s.UFD[  1,  1]' " ~
                         "holds directory [%3s,%3s] " ~
                         "with %4d files in %4d slots "~
                         "t0=%d "~
                         "needs %d track%s UFD_tracks_used=%d",
                         i,u[3..6]~u[0..3],u[3..6],u[0..3],ufd_filecount[u],ufd_filecontent[u].length,
                         mfd[i].track,
                         tracks_needed, tracks_needed==1 ?"   ":"s  ",
                         ufd_tracks_used );
    store_directory_file( ufd_filecontent[u], mfd[i].track, sixbit_ppn );
  }
  store_directory_file( mfd, 1, sixbit_1_1 );
  stdout.writefln("There are %6d UFD files and %6d ordinary files",ufdcnt,file_count);
  stdout.writefln("The ordinary files occupy tracks [%d..%d]",data_track,free_track); 
  setsatbits(0,ufd_tracks_used+1);
  setsatbits(data_track,free_track);
  // if(1){stdout.writefln("\nEXIT early return(0)");return(0);}
  track[0].sat_head.dskuse      = 1 + ufd_tracks_used + (free_track-data_track);
  track[0].sat_head.lstblk      = free_track-1;
  // MFD directory entry slot#0 of itself in track#1 as filename '  1  1.UFD'
  track[1].ufdent[0].filnam     = sixbit_1_1;
  track[1].ufdent[0].ext        = sixbit_ufd;
  track[1].ufdent[0].track      = 1;
  // Write binary disk image using 64-bits for each 36-bit PDP-10 word.
  stdout.writeln("\n===  Writing ./KIT/DASD.data8 ===");
  auto outfile = File("./KIT/DASD.data8","wb");
  track.length = free_track+1;
  outfile.rawWrite(track);
  outfile.close();
  stdout.writefln("\n=== DONE ===");
  return(0);
}
