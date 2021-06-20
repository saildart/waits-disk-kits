#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
""" PAGE 1
program:        maud.py                 Make.All.User.Directories
synopsis:       maud {kitpath}
debug:          py -m pudb maud.py $KIT
description:    For a disk kit convert the UCFS tree of files into a DASD.data8 disk image.
        Observe fresher text file time stamps and replace stale (or non-existent) data8 binary.        
ruler: ========================================================================================= 100 ============= 118
steps:  ● Fetch absolute {kitpath} from argument argv[1]
        ● Remove old MFD tree at its root {kitpath}/UCFS/1.1
        ● Make empty MFD directory : root {kitpath}/UCFS/1.1
        ● Walk the UCFS tree looking for fresh TEXT to convert back into DATA8 files..
        ● Rebuild the model SAIL-WAITS User_File_Directories into 1.1/.PRGPRJ.UFD files..
        ● Allocate absolute track numbers to each file
        ● Write the Storage_Allocation_Table, SAT bitmap, into DASD track x000000
        ● Copy DATA8 file segments from UCFS into DASD tracks
                with proper 32-word RIBS for each 2304-word payload.
symlink: /usr/local/bin/maud is the "installed" copy.
license: MIT License Copyright (c)2021 Bruce Guenther Baumgart.
"""
import datetime
import os
import pathlib
#import pudb
import pdb
import re
import shutil
import struct
import sys
import time
# todo: □ separate Source from Installed python
# local installed is at /usr/local/lib/python3.8/dist-packages/
import utf8_into_data8 as cv  # replace with "utf8_x_data8"
import mathilda as mt         # subordinate (or obsolete) code

class Struct():
    "class for item.dot.field notation like'C'proglang structures"
    
class Kit():
    "class for bucket of variables related to a SAIL-WAITS kit model"
    
kit = Kit()

def stop():
    sys.stdout.close()
    sys.stderr.close()
    os._exit(0)
    
def bp():
    pdb.set_trace()
    
def bpu():
    pudb.set_trace()
    
def vprint(*args,**kwargs):
    print(*args,**kwargs)
    pass

def qprint(*args,**kwargs):
    pass

def maud():
    vprint("PAGE 1    ● kitpath from argument.        ● Create empty MFD.")

    if(len(sys.argv)!=2):
        print("synopsis: maud Kitpath")
        os._exit(1)
    kit.path = os.path.realpath( sys.argv[1] )
    kit.rootpath = kit.path + "/UCFS"
    kit.mfd_path = kit.path + "/UCFS/1.1"
    kit.ufd_list = []
    kit.ufd_dict = {}           # directory dictionary keyed on {programmer} dot {project} strings
    kit.data8_dict = {}         # ALL files keyed on dot slash path name
    kit.matepair_list = []
    kit.fat = [] # file at track# list
    
    """ UCFS model pattern definitions """
    pat = Struct()
    pat.dotppn = re.compile(r"\. / (?P<prg>\w{1,3})       \.(?P<prj>\w{1,3})"                        ,re.X)
    pat.ppn    = re.compile(r"     (?P<prg>\w{1,3})       \.(?P<prj>\w{1,3})"                        ,re.X)
    pat.filnam = re.compile(r"     (?P<filnam>[\w]{1,6}) (\.(?P<ext>\w{0,3}))? (\.(?P<tbx>[tbx]))? $",re.X)
    pat.dotnam = re.compile(r"\.   (?P<filnam>[\w]{1,6}) (\.(?P<ext>\w{0,3}))? (\.(?P<tbx>[tbx]))? $",re.X)
    pat.txtnam = re.compile(r"     (?P<filnam>[\w]{1,6}) (\.(?P<ext>[A-Z0-9]{0,3}))?               $",re.X)
    
    """ step: ● Remove old MFD tree. """
    """ step: ● Create new empty MFD. """
    try:
        shutil.rmtree( kit.mfd_path )
    except OSError as e:
        pass
    pathlib.Path( kit.mfd_path ).mkdir( parents=True, exist_ok=True )

    vprint(
    """PAGE 2
    step: ● Find TEXT files that are newer than DATA8.
    step: ● Replace stale DATA8.
    """)
    """
    todo: □ Consider whether to remove directories and files that fail UCFS-model pattern matching.
    todo: □ Find DATA8 which lack TEXT.
    todo: □ Generat TEXT mate for each DATA8 widow.
    """
    os.chdir( kit.rootpath )
    for dot_ufd, dirs, files in os.walk("."):
        dirs.sort()
        files.sort()
        qprint("\nWalking top {}\n        subdir {}\n        files {}".format( dot_ufd, dirs, files))
        pp = dot_ufd[2:] # programmer dot project key
        match = pat.dotppn.match( dot_ufd )
        if not match : continue
        #
        ufd = Struct()
        if len(kit.ufd_list) == 0 :
            mfd = ufd
            assert( pp == "1.1" )
        kit.ufd_list.append( ufd )
        kit.ufd_dict[pp] = ufd        
        prj  = match.group('prj')
        prg  = match.group('prg')
        assert( pp == "".join([prg,".",prj]))        
        ppn  = ("{:>3}{:>3}".format(prj,prg)).upper()
        ppn_ = ("{:_>3}{:_>3}".format(prg,prj)).upper()
        ppn6b= "".join(["%02o"%(ord(x)-32) for x in ppn])
        #
        ufd.path = "./1.1/.{}.UFD".format( ppn_ )
        qprint("NEW directory file {}".format( ufd.path ))
        kit.data8_dict[ ufd.path ] = ufd
        ufd.pp = pp
        ufd.prg_dot_prj = pp
        ufd.file_words  = 0
        ufd.file_tracks = 0
        ufd.files       = [ f for f in files if pat.filnam.match( f ) ]
        ufd.file_count  = len(ufd.files)
        ufd.files.sort()        
        for f in ufd.files:
            #
            txt = Struct()
            txt.path = dot_ufd+"/"+f
            txt.date = os.path.getmtime( txt.path )
            txt.isodate = datetime.datetime.fromtimestamp(txt.date).strftime('%Y-%m-%d %H:%M:%S')
            #
            data8 = Struct()
            data8.path = dot_ufd+"/."+(f[:-2] if f.endswith(".x") or f.endswith(".b") else f)
            kit.data8_dict[ data8.path ] = data8
            data8.exists = os.path.exists( data8.path )
            match = pat.filnam.match( f ); assert( match ) # 2nd time for sure is good.
            qprint("NEW  regular  file  named {}".format(data8.path))
            #
            if data8.exists:
                data8.date = os.path.getmtime( data8.path )
            if (not data8.exists or data8.date < txt.date) and pat.txtnam.match( f ) :
                # refresh content of data8 file when stale
                cv.utf8_into_data8( txt.path, data8.path )
                data8.exists = os.path.exists( data8.path );assert(data8.exists)
            if data8.exists:                
                data8.date = os.path.getmtime( data8.path )
                data8.isodate = datetime.datetime.fromtimestamp(data8.date).strftime('%Y-%m-%d %H:%M:%S')
                data8.words = os.path.getsize( data8.path ) // 8
                data8.tracks = (data8.words + 2303) // 2304                
            # mating TEXT and DATA8 file name pairs
            ufd.file_words  += data8.words
            ufd.file_tracks += data8.tracks
            data8.txt = txt
            txt.data8 = data8
            kit.matepair_list.append( [data8,txt] )
        # end-of-loop for f
        qprint("dot_ufd  {:10} has {:4} files requiring {:4} tracks".
              format( dot_ufd, len(ufd.files), ufd.file_tracks ))
    # end-of-loop for dot_ufd

    vprint("""PAGE 3    ● Track allocation """)
    ufdcnt = len( kit.ufd_list )
    dircnt = len( kit.ufd_dict )
    filcnt = len( kit.matepair_list )
    totcnt = dircnt + filcnt
    
    mfd.file_count = dircnt
    mfd.track  = 0
    mfd.tracks = (mfd.file_count + 575) // 576
    next_track = mfd.track + mfd.tracks
    
    # Allocate tracks for SAIL directory files, the /1.1/PRGPRJ.UFD files
    for u in kit.ufd_list:
        u.track  = next_track
        u.tracks = (u.file_count + 575) // 576  # Directory size is file_count × 4-words per slot
        u.groups = (u.tracks+31)//32
        u.words  = 4*u.file_count
        next_track += u.tracks
        kit.fat.append( u )
        
    # Allocate tracks for SAIL regular files
    for [d,t] in kit.matepair_list:
        d.track  = next_track
        d.tracks = (d.words + 2304) // 2304 # File size in PDP-10 words
        d.groups = (d.tracks+31)//32
        next_track += d.tracks
        kit.fat.append( d )

    # Eyeball the allocation
    vprint(
        "    Allocated {} UFDs representing {} SAIL_directories with {} SAIL_files,\n".format( ufdcnt, dircnt, filcnt ) +
        "    for a total  of {} SAIL_objects using {} tracks including the track#0 SAT table.\n".format( totcnt, next_track ) +
        "    len(kit.fat) is {} objects".format( len(kit.fat) )
        )
    i=0
    if(1):
        for u in kit.ufd_list:
            i+=1
            vprint("{:6}.directory {:7} {:3} file{}        length {:4} track{} starting at track #{:06} is in SAIL_file {:24}".
                  format( i,
                          u.pp,
                          u.file_count,"s "[u.file_count==1],
                          u.tracks,    "s "[u.tracks==1],
                          u.track, u.path ))
    if(1):
        vprint("")
        for [d,t] in kit.matepair_list:
            i+=1
            gstring = ["","{} groups".format(d.groups)][ d.groups > 1 ]
            vprint("{:6}.SAIL_file {:24} length {:4} track{} starting at track #{:06} {}".
                  format( i,
                          d.path,
                          d.tracks, "s "[d.tracks==1],
                          d.track, gstring ))
    # bp()
    print(len(kit.fat))
    if(1):
        i=0
        for o in kit.fat:
            i+=1
            gstring = ["","{} groups".format(o.groups)][ o.groups > 1 ]
            vprint("{:6}. path   {:24}  in {:4} track{} starting at track #{:06} {}".
                   format(i,
                          o.path[2:],
                          o.tracks, "s "[o.tracks==1],
                          o.track, gstring ))
    # stop()
    
    vprint("""\n PAGE 4    ● Write new UFD files    """)
    vprint("""\nstep: ● Write new ./1.1/{programmer}{project}.UFD files  """)
    os.chdir( kit.rootpath ) # redundant reminder, doesn't work for dot relative
    mfd.path = "./1.1/.__1__1.UFD"
    mfd.file = open( mfd.path, 'wb' )
    dirlist = os.listdir( "." )
    dirlist.sort()
    for dir in dirlist:
        match = pat.ppn.match( dir )
        if not match : continue
        prj = match.group('prj')
        prg = match.group('prg')
        pp  = "".join([prg,".",prj])        
        ufd = kit.ufd_dict[ pp ]
        ppn  = ("{:>3}{:>3}".format(prj,prg)).upper()
        ppn_ = ("{:_>3}{:_>3}".format(prg,prj)).upper()
        ppn6b= "".join(["%02o"%(ord(x)-32) for x in ppn])
        if(1):vprint("pp {:7}  dir {:10}  ppn_ {:10}   ppn {:10}  {:>10} {:>3} {:10} {:10}".
              format(pp,dir,ppn_,ppn,prj,prg,ppn,ppn6b))
        if not ppn_ == '__1__1' :            
            #{ pycurly_block ================================================================================
            ufd.path = "./1.1/.{}.UFD".format( ppn_ )
            ufd.file = open( ufd.path, 'wb' )
            filelist = os.listdir( './'+dir )
            filelist.sort()
            for dotfil in filelist:
                hit = pat.dotnam.match( dotfil )
                if not hit : continue
                fn = hit.group('filnam')
                ex = hit.group('ext'); ex = (ex,"")[ex is None]
                bx = hit.group('tbx'); bx = (bx,"")[bx is None]
                if(ex): ex = "."+ex 
                if(bx): bx = "."+bx 
                #vprint("found {:>32} {:<32}  fn'{:}' ex'{:}' bx'{:}'".format( dir, dotfil, fn, ex, bx ))
                data8 = kit.data8_dict[ './'+dir+'/'+dotfil ]
                data8.size = os.path.getsize( data8.path ) // 8
                mtime = os.stat( data8.path ).st_mtime
                dt = time.localtime( mtime )
                dt_str   = time.strftime("%Y-%m-%d %H:%M:00", dt )
                saildate = ((max(0,(dt.tm_year-1964))*12+dt.tm_mon-1)*31+dt.tm_mday-1)
                sailtime = dt.tm_hour*60 + dt.tm_min
                created  = saildate # stop-gap. ToDo: fetch created from dart mysql table.
                prot     = 0o000 # Don't bother for 21st century modeling. Curators may see everything.
                mode = { "":0, ".x":0o10, ".b":0o17 }[ bx ] # neo_mode_code            
                fil = "{0:<6}".format(hit.group('filnam').upper())
                ext = hit.group('ext') or "   "
                ext = "{0:<3}".format(ext.upper())
                data8.fil6b = "".join([ "%02o"%(ord(x)-32) for x in fil ])
                data8.ext6b = "".join([ "%02o"%(ord(x)-32) for x in ext ])
                data8.ppn6b = ppn6b
                # UFD slot content for regular SAIL file
                # ------ | ------ | ------ || ------ | ------ | ------ # 36-bits as 6 x 6
                #    F        I        L         N        A        M   # SIXBIT / { filnam } /
                #    E        X        T      mdate_hi(3)  created(15) # SIXBIT / { ext } /
                #   prot(9)   mode(4)   mtime(11)         mdate_lo(12) #
                # --------- | ----    | -----------   |   ------------ #
                #                                            Track#    # max track in 1974 was 18-bits
                # ---------------------------------------------------- #
                w =  int(data8.fil6b,8)            
                x = (int(data8.ext6b,8) << 18) | ((saildate & 0o70000)<<3) | created
                y = (prot << 27) | (mode << 23) | ((sailtime & 0o3777)<<12) | (saildate & 0o7777)
                z = data8.track
                if(1):
                    vprint("UFD_slot {:>8} {:6} {:3}   ppn_ {} into {:15.15} track #{:06}".
                          format( dir, fil, ext, ppn_, ufd.path[-15:], z ))
                UFD_item = struct.pack("4Q",w,x,y,z);
                ufd.file.write( UFD_item )
            ufd.file.close()
            #} pycurly_block ================================================================================        
        # MFD slot content for SAIL directory
        # ------ | ------ | ------ || ------ | ------ | ------ # 36-bits as 6 x 6
        #    P        R        J         P        R        G   # This is SAIL [ project , programmer ] order.
        #   "U"      "F"      "D"     mdate_hi(3)  created(15) # The extension is SIXBIT/UFD/ octal_65_46_44
        #   prot(9)   mode(4)   mtime(11)         mdate_lo(12) #
        # --------- | ----    | -----------   |   ------------ #
        #                                            Track#    #
        # ---------------------------------------------------- #
        dt = time.localtime()
        now_str = time.strftime("%Y-%m-%d %H:%M:00", dt )
        saildate = ((max(0,(dt.tm_year-1964))*12+dt.tm_mon-1)*31+dt.tm_mday-1)
        sailtime = dt.tm_hour*60 + dt.tm_min
        created  = saildate
        prot = 0
        mode = 0o17
        w = int(ppn6b,8)
        x = ( 0o_65_46_44 << 18)        | ((saildate & 0o70000)<<3) | created
        y = (prot << 27) | (mode << 23) | ((sailtime & 0o3777)<<12) | (saildate & 0o7777)
        z = kit.ufd_dict[ pp ].track
        if(1): vprint("MFD_slot {:>8}'{:6}'{:3}   ppn_ {} into {:15.15} track #{:06}".
                     format("1.1", ppn, "UFD", ppn_, mfd.path[-15:], z ))
        MFD_item = struct.pack("4Q",w,x,y,z);
        mfd.file.write( MFD_item )
        ufd.fil6b = ppn6b
        ufd.ext6b = "654644"
        ufd.ppn6b = "000021000021"
    mfd.file.close()

    vprint("""\n PAGE 5  ● Write SAT bitmap into DASD Track Zero """)
    """
================================================================================
;WAITS 6.17J source code from ALLDAT[SYS,J17] starting at line 168:
;SAT TABLE AS STORED ON BLOCK 0 OF THE DISK
	DEFINE ZWD (A)<↑A←←.-SATTAB ↔ 0 >
	DEFINE ZLOC (A)<↑A←←.-SATTAB>
↑SATTAB:			;THIS IS SAT BIT AREA AS STORED ON DISK.
	ZWD(DSKUSE)		;BLOCKS USED ON DISK
	ZWD(LSTBLK)		;NUMBER OF LAST BLOCK ASSIGNED
	ZWD(SATID)		;IDENT.NO. OF ALL DISK INFO
	ZWD(SATCHK)		;XOR CHECKSUM OF SAT BITS BELOW
	ZWD(BADCNT)		;NO. OF BAD TRACKS IN TABLE BELOW.
	ZWD(BADCHK)		;CHECK (SUM) OF TABLE.
	ZLOC(BADTRK)
	BLOCK BADMAX		;TABLE OF BAD LOGICAL BLOCK NUMBERS (TRACKS)
	ZWD(IDSAT)		;CONTAINS 'SATID ' FOR NEW UDPS
	ZWD(DTIME)		;TIME SAT LAST WRITTEN
	ZWD(DDATE)		;DATE SAT LAST WRITTEN
↑P1OFF:		0		;# PDP-10 POWER FAILURES	(MAIN SAT ONLY!)
↑P2OFF:		0		;# OF PDP-6 POWER FAILURES	(MAIN SAT ONLY)
		0		;BUFFER WORD.			(NOT NEEDED)
	ZLOC(SATBIT)		;LOCATION OF THE BIT TABLE ITSELF
	BLOCK SATWCT	
↑SATEND:	0		;END OF MAIN BIT TABLE (UDP SATS ARE DIFFERENT SIZE)
↑SATSIZ←←SATEND-SATTAB
    """
    dasd = open( kit.path+"/DASD.data8", "wb" )
    
    # The PDP10 bit positions within a word are numbered from left to right
    BIT = list(1<<n for n in range(35,-1,-1))
    MASK = list(2**n-1 for n in range(37))
    MASK_big_endian = list((2**n-1)<<(36-n) for n in range(37))
    
    t0 = Struct()
    t0.dskuse = next_track
    t0.lstblk = next_track-1
    t0.satid = 0o3164236
    t0.satchk = 0
    t0.badcnt = 0
    t0.badchk = 0
    t0.badtrk = [0 for i in range(45)]
    t0.idsat = 0
    t0.dtime = 0
    t0.ddate = 0
    t0.p1off = 0
    t0.p2off = 0
    t0.spare = 0
    setsat_words = t0.dskuse // 36
    setsat_bits = MASK_big_endian[ t0.dskuse % 36 ]
    t0.satbit = \
        [ MASK[36] for i in range(setsat_words) ] + \
        [ setsat_bits ] + \
        [ 0 for i in range(1267-setsat_words-1) ]
    t0.unused = [0 for i in range(980 + 32)]
    # WAITS 6.17 concatenates the RIB and PAYLOAD for Track Zero;
    # and it only reads/writes the first 1324 words of the 2336 word track.
    track_zero = struct.pack("2336Q",
                             t0.dskuse, t0.lstblk, t0.satid, t0.satchk, t0.badcnt, t0.badchk,
                             *t0.badtrk,
                             t0.idsat, t0.dtime, t0.ddate, t0.p1off, t0.p2off, t0.spare,
                             *t0.satbit,
                             *t0.unused )
    dasd.write( track_zero )

    vprint(""" PAGE 6  ● Write new DASD Tracks 1 to {} inclusive.""".format(t0.lstblk))
    """
    struct RIB{
      //         Renamed              Ralph.FAI name
      //         ----------------     ----------------------------------------------------------------
      WORD_PDP10 filnam,              // DDNAM sixbit
                 ext,                 // DDEXT sixbit
                 prot,                // DDPRO SAIL protection bits always set to zero here
                 ppn,                 // DDPPN sixbit
                 location,            // DDLOC as track#
                 filesize,            // DDLNG in PDP-10 words !
                 ref_datetime,        // DREFTM
                 dmp_datetime,        // DDMPTM
                 group_one_relative,  // DGRP1R 1 + group_index × 32 × 18
                 next_group_track,    // DNTXGP zero or track# of first RIB of next group.
                 satid;               // DSATID 03164236
      WORD_PDP10[5] dqinfo;           // DQINFO special information (passwords, WRTPPN, WRTOOL)
      WORD_PDP10[16] dptr;            // DPTR   32. Track numbers for this group,
                                      // which are packed left then right, BIG-endian PDP-byte-order.
    } //         ----------------     ----------------------------------------------------------------
    """
    rib = Struct()
    rib.filnam, rib.ext, rib.prot, rib.ppn, rib.location,\
    rib.filesize, rib.ref_date, rib.dmp_date,\
    rib.group_one_relative, rib.next_group_track,\
    rib.satid = (0 for i in range(11))
    rib.info = [0 for i in range( 5)]
    rib.dptr = [0 for i in range(16)]    

    cnt = 0
    for o in kit.fat:
        cnt += 1
        data8 = kit.data8_dict[ o.path ]
        size = os.path.getsize( o.path ) // 8
            
        # Initialize the prime RIB for this file
        rib.filnam = int(data8.fil6b,8)
        rib.ext    = int(data8.ext6b,8) << 18
        rib.prot   = 0
        rib.ppn    = int(data8.ppn6b,8)
        rib.location =      o.track
        rib.filesize = size
        rib.ref_date = 0
        rib.dmp_date = 0
        rib.satid = 0o3164236 if rib.ppn==0o21000021 else 0
            
        f = open( o.path, 'rb' )
        group_count, track_final = divmod( o.tracks, 32 )
        tracks = [i for i in range( o.track, o.track + o.tracks )]
        groups = [tracks[i+0:i+32] for i in range(0,len(tracks),32)]
        q = 0
        p = 32
        for g in groups :
            table = g + [0 for i in range(32-len(g))]
            rib.dptr = [((table[i]<<18)+table[i+1]) for i in range(0,32,2)] # Big endian half words
            rib.group_one_relative = 1 + q*32*18 # SAIL WAITS mickey mouse magic number
            if p < len(tracks):
                rib.next_group_track = tracks[p]
            else:
                rib.next_group_track = 0
            q += 1
            p += 32                
            for t in g:
                buf = f.read(2304*8)
                ribbon = struct.pack("32Q", rib.filnam, rib.ext, rib.prot, rib.ppn,
                                     rib.location, rib.filesize, rib.ref_date, rib.dmp_date,
                                     rib.group_one_relative, rib.next_group_track,
                                     rib.satid, *rib.info, *rib.dptr )
                payload = buf + bytes(8*2304 - len(buf))
                dasd.write( ribbon + payload )                
        f.close()
        gstring = ["","{} groups".format(o.groups)][ o.groups > 1 ]
        vprint("{:6}.        {:24}  in {:4} track{} starting at track #{:06} {}".
               format(cnt,
                      o.path[2:],
                      o.tracks, "s "[o.tracks==1],
                      o.track, gstring ))
            
    """ Finished """
    dasd.close()
    print("="*86+"\ndone: ▶ maud( {} ); done.".format(kit.path))
    
if __name__ == "__main__" :
    maud()    
""" lpr -o media=letter -o page-left=48 -o cpi=16 -o lpi=10 maud.py """

