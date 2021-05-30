#!/usr/bin/python3
# -*- mode: python; coding: utf-8 -*-
"""
program:        maud.py                 Make.All.User.Directories
synopsis:       maud {kitpath}
debug:          py -m pudb maud.py $KIT
description:    Maintain model kits of the SAIL-WAITS file system derived from the SAILDART archive.
ruler: ========================================================================================= 100
steps:  ● Fetch absolute {kitpath} from argument argv[1]
        ● Remove old MFD tree at its root {kitpath}/UCFS/1.1
        ● Make empty MFD directory : root {kitpath}/UCFS/1.1
        ● Walk the UCFS tree looking for fresh TEXT to convert back into DATA8 files.
.
        ● Collect database rows with
                PRG, PRJ, FILNAM, EXT, size, TBX, sn, created_date, written_date_time
        ● Rebuild the model SAIL-WAITS User_File_Directories into 1.1/.PRGPRJ.UFD files.
.
        ● Allocate absolute track numbers to each file
        ● Write the Storage_Allocation_Table, SAT bitmap, into DASD track x000000
        ● Copy DATA8 file segments from UCFS into DASD tracks
                with proper 32-word RIBS for each 2304-word payload.
symlink: /usr/local/bin/maud points to an "installed" source copy.
license: MIT License Copyright (c)2021 Bruce Guenther Baumgart.
"""
import datetime
import os
import pathlib
import pudb
import re
import shutil
import struct
import sys
import time

import utf8_into_data8 as cv    # replace with "utf8_x_data8"
import matilda as mt            # subordinate or obsolete code

class Struct():
    "class for item.dot.field notation like'C'proglang structures"
class Kit():
    "class for bucket of variables related to a SAIL-WAITS kit model"
    
# global
kit = Kit()

def stop():
    sys.stdout.close()
    sys.stderr.close()
    os._exit(0)
def bp():
    pudb.set_trace()
def maud():
    """ step:   ● kitpath from argument.  """
    if(len(sys.argv)!=2):
        print("synopsis: maud Kitpath")
        os._exit(1)
    kit.path = sys.argv[1]
    kit.rootpath = kit.path + "/UCFS"
    kit.mfd_path = kit.rootpath + "/1.1"
    kit.ufd_list = []
    kit.ufd_dict = {} # directory dictionary keyed on {programmer} dot {project} strings
    kit.matepair_list = []
    
    """ UCFS model pattern definitions """
    pat = Struct()
    pat.dotppn = re.compile(r"\. / (?P<prg>\w{1,3})       \.(?P<prj>\w{1,3})"                        ,re.X)
    pat.ppn    = re.compile(r"     (?P<prg>\w{1,3})       \.(?P<prj>\w{1,3})"                        ,re.X)
    pat.filnam = re.compile(r"     (?P<filnam>[\w]{1,6}) (\.(?P<ext>\w{0,3}))? (\.(?P<tbx>[tbx]))? $",re.X)
    pat.dotnam = re.compile(r"\.   (?P<filnam>[\w]{1,6}) (\.(?P<ext>\w{0,3}))? (\.(?P<tbx>[tbx]))? $",re.X)
    pat.txtnam = re.compile(r"     (?P<filnam>[\w]{1,6}) (\.(?P<ext>\w{0,3}))?                     $",re.X)
    
    """steps:  ● Remove old MFD tree.  ● Create empty MFD.  """
    try:
        shutil.rmtree( kit.mfd_path )
    except OSError as e:
        pass
    pathlib.Path( kit.mfd_path ).mkdir( parents=True, exist_ok=True )
    
    """step:   ● Find synthetic TEXT newer than DATA8.   ● Replace stale DATA8.    """
    """todo:   □ Consider whether to remove directories and files that fail the model-pattern match. """
    """todo:   □ Find DATA8 which lack TEXT.  □ Generat TEXT mate for each DATA8 widow. """
    os.chdir( kit.rootpath )
    for dot_ufd, dirs, files in os.walk("."):
        dirs.sort()
        # files.sort()
        # print("\nWalking top {}\n        subdir {}\n        files {}".format(dot_ufd,dirs,files))
        if not pat.dotppn.match( dot_ufd ):
            continue
        ufd = Struct()
        pp = dot_ufd[2:] # programmer dot project key
        kit.ufd_list.append( ufd )
        kit.ufd_dict[pp] = ufd        
        ufd.pp = pp
        ufd.prg_dot_prj = pp
        ufd.file_words  = 0
        ufd.file_tracks = 0
        ufd.files       = [ f for f in files if pat.filnam.match( f ) ]
        ufd.file_count  = len(ufd.files)
        ufd.files.sort()
        for f in ufd.files:
            txt = Struct()
            txt.path = dot_ufd+"/"+f
            txt.date = os.path.getmtime( txt.path )
            txt.isodate = datetime.datetime.fromtimestamp(txt.date).strftime('%Y-%m-%d %H:%M:%S')
            data8 = Struct()
            data8.path = dot_ufd+"/."+f
            data8.exists = os.path.exists( data8.path )
            if data8.exists:
                data8.date = os.path.getmtime( data8.path )
                data8.isodate = datetime.datetime.fromtimestamp(data8.date).strftime('%Y-%m-%d %H:%M:%S')
                data8.words = os.path.getsize( data8.path ) // 8
                data8.tracks = (data8.words + 2303) // 2304
            if (not data8.exists or data8.date < txt.date) and pat.txtnam.match( f ) :
                # refresh content of data8 file when stale
                print("{:24} {}  {}".format( txt.path, txt.isodate, data8.isodate))
                cv.utf8_into_data8( txt.path, data8.path )
                # new data8 exists
                data8.exists = os.path.exists( data8.path );assert(data8.exists)
                data8.date = os.path.getmtime( data8.path )
                data8.isodate = datetime.datetime.fromtimestamp(data8.date).strftime('%Y-%m-%d %H:%M:%S')
                data8.words = os.path.getsize( data8.path ) // 8
                data8.tracks = (data8.words + 2303) // 2304
            # mating
            ufd.file_words  += data8.words
            ufd.file_tracks += data8.tracks
            data8.txt = txt
            txt.data8 = data8
            kit.matepair_list.append( [data8,txt] )
            
        print("dot_ufd  {:10} has {:4} files requiring {:4} tracks".
              format( dot_ufd, len(ufd.files), ufd.file_tracks ))
                
    # do we know enough now to allocate tracks ?
    ufdcnt = len( kit.ufd_list )
    dircnt = len( kit.ufd_dict )
    filcnt = len( kit.matepair_list )
    totcnt = dircnt + filcnt
    print("{} ufd as ( {} SAIL_directories with + {} SAIL_files ) is {} total SAIL_objects\n".
          format( ufdcnt, dircnt, filcnt, totcnt ))
    mfd = kit.ufd_list[0]
    kit.mfd = mfd
    mfd.file_count = dircnt
    mfd.track  = 0
    mfd.tracks = (mfd.file_count + 575) // 576
    next_track = mfd.track + mfd.tracks
    
    # Allocate track start locations for all the SAIL files
    for u in kit.ufd_list:
        u.track  = next_track
        u.tracks = (u.file_count + 575) // 576
        u.words  = 4*u.file_count
        next_track += u.tracks
    for [d,t] in kit.matepair_list:
        d.track  = next_track
        d.tracks = (d.words + 2304) // 2304
        next_track += d.tracks
        
    # Eyeball the allocation
    if(1):
        for u in kit.ufd_list:
            print("directory {:7} {:3} file{}        length {:4} track{} starting at track #{:06}".
                  format( u.pp,
                          u.file_count,"s "[u.file_count==1],
                          u.tracks,    "s "[u.tracks==1],
                          u.track ))        
    if(0):
        for [d,t] in kit.matepair_list:
            d.groups = (d.tracks+31)//32
            gstring = ["","{} groups".format(d.groups)][ d.groups > 1 ]
            print("SAIL_file {:24} length {:4} track{} starting at track #{:06} {}".
                  format( d.path,
                          d.tracks, "s "[d.tracks==1],
                          d.track, gstring ))
    # bp()
    # stop()
    
    print("""\nstep: ● Write new /1.1/{programmer}{project}.UFD files  """)
    # os.chdir( kit.rootpath ) # redundant reminder
    mfd.path = "{}/.__1__1.UFD".format( kit.mfd_path )
    mfd_file = open( mfd.path, 'wb' )
    dirlist = os.listdir( kit.rootpath )
    dirlist.sort()
    for dir in dirlist:
        match = pat.ppn.match( dir )
        if not match :
            continue
        ufd  = kit.ufd_dict[pp]
        prj  = match.group('prj')
        prg  = match.group('prg')
        pp   = "".join([prg,".",prj])
        ppn  = ("{:>3}{:>3}".format(prj,prg)).upper()
        ppn_ = ("{:_>3}{:_>3}".format(prg,prj)).upper()
        ppn6b= "".join(["%02o"%(ord(x)-32) for x in ppn])
        # bp()
        if(1):print("pp {:7}  dir {:10}  ppn_ {:10}   ppn {:10}  {:>10} {:>3} {:10} {:10}".
              format(pp,dir,ppn_,ppn,prj,prg,ppn,ppn6b))
        """
        The MFD is nearly empty, except for the mfd_file we just opened.
        Avoid over writing the mfd_file with its own ufd.file.
        """
        if not ppn_ == '__1__1' :            
            #{ pycurly_block ================================================================================
            ufd.path = "{}/.{}.UFD".format( kit.mfd_path, ppn_ )
            ufd.file = open( ufd.path, 'wb' )
            filelist = os.listdir( kit.rootpath+'/'+dir )
            filelist.sort()
            for dotfil in filelist:
                hit = pat.dotnam.match( dotfil )
                if not hit :
                    # print("ignore {:>32} {:<32}".format( dir, dotfil ))
                    continue
                fn = hit.group('filnam')
                ex = hit.group('ext'); ex = (ex,"")[ex is None]
                bx = hit.group('tbx'); bx = (bx,"")[bx is None]
                if(ex): ex = "."+ex 
                if(bx): bx = "."+bx 
                #print("found {:>32} {:<32}  fn'{:}' ex'{:}' bx'{:}'".format( dir, dotfil, fn, ex, bx ))    
                data8.path = kit.rootpath+'/'+dir+'/'+dotfil
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
                fil6b = "".join([ "%02o"%(ord(x)-32) for x in fil ])
                ext6b = "".join([ "%02o"%(ord(x)-32) for x in ext ])            
                # ---------------------------------------------------- # UFD slot content
                # ------ | ------ | ------ || ------ | ------ | ------ # 36-bits as 6 x 6
                #    F        I        L         N        A        M   #
                #    E        X        T      mdate_hi(3)  created(15) #
                #   prot(9)   mode(4)   mtime(11)         mdate_lo(12) #
                # --------- | ----    | -----------   |   ------------ #
                #                                            Track#    # max track in 1974 was 18-bits
                # ---------------------------------------------------- #
                w =  int(fil6b,8)            
                x = (int(ext6b,8) << 18) | ((saildate & 0o70000)<<3) | created
                y = (prot << 27) | (mode << 23) | ((sailtime & 0o3777)<<12) | (saildate & 0o7777)
                z = 0
                if(0):
                    print("UFD_slot {:>8} {:6} {:3}   ppn_ {} into {:15.15}".
                          format( dir, fil, ext, ppn_, ufd.path[-15:] ))
                UFD_item = struct.pack("4Q",w,x,y,z);
                ufd.file.write( UFD_item )
            ufd.file.close()
            #} pycurly_block ================================================================================
        
        # ----------------------------------------------------------- # MFD slot content
        #   P R J P R G     {project} {programmer}
        #   U F D           mdate_hi(3) cdate(15)
        #                   prot(9) mode(4) mtime(11) mdate_lo(12)
        #   Track#
        # ----------------------------------------------------------- #
        dt = time.localtime()
        now_str = time.strftime("%Y-%m-%d %H:%M:00", dt )
        saildate = ((max(0,(dt.tm_year-1964))*12+dt.tm_mon-1)*31+dt.tm_mday-1)
        sailtime = dt.tm_hour*60 + dt.tm_min
        created  = saildate
        prot = 0
        mode = 0o17
        w =  int(ppn6b,8)
        x = ( 0o_65_46_44 << 18)        | ((saildate & 0o70000)<<3) | created
        y = (prot << 27) | (mode << 23) | ((sailtime & 0o3777)<<12) | (saildate & 0o7777)
        z = kit.ufd_dict[pp].track
        print("MFD_slot {:>8}'{:6}'{:3}   ppn_ {} into {:15.15}".format("1.1", ppn, "UFD", ppn_, mfd.path[-15:] ))
        MFD_item = struct.pack("4Q",w,x,y,z);
        mfd_file.write( MFD_item )
    mfd_file.close()

    dasd = open(kit.path+"/DASD.data8","wb")
    dasd.close()
    
    """ Finished """
    print("="*86+"\nmaud( {} ); done.".format(kit.path))
    
if __name__ == "__main__" :
    maud()    
'''
'''
