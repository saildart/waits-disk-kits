#!/usr/bin/python3
"""
Program: maud.py        Make All User Directories.

Synopsis: Walk the UCFS looking for
        synthetic TEXT files that are newer than the authentic DATA8 files.
        Revert the fresh file into DATA8 format replace authentic with synthetic DATA8 content.
        Adjust the UFD file slots in the SAIL-WAITS root [1,1] area to point at the newer content.
        Maud updates the *.UFD [1,1] files that model the SAIL WAITS Master File Directory.

Trivia: The intersection of Maud and Matilda in Sunnyvale, central to Silicon Valley,
        is named for two women born in the 19th century, member of the extensive Murphy family
        which once owned, settled and farmed the land that became Sunnyvale, California.
"""
import re
import os
import pathlib
import struct
import time
import datetime

root = os.getenv('SYS')

pattern_directory = re.compile(r"(?P<prg>\w{1,3})\.(?P<prj>\w{1,3})",re.X)
pattern_prg_dot_prj = re.compile(r"\./(?P<prg>\w{1,3})\.(?P<prj>\w{1,3})",re.X)
pattern_dot_filename = re.compile(r"\.(?P<filnam>[\w ]{1,6})(\.(?P<ext>\w{0,3}))?$",re.X)
pattern_filename = re.compile(r"(?P<filnam>[\w ]{1,6})(\.(?P<ext>\w{0,3}))?$",re.X)

os.chdir(root)
for ufd, dirs, files in os.walk(".", topdown = True):
    dirs.sort()
    dirs = [ d for d in dirs if pattern_directory.match( d ) ]
    if dirs : print("dirs {}".format(dirs))
    print("ufd = "+ufd)
    if pattern_prg_dot_prj.match( ufd ):
        files.sort()
        files = [ f for f in files if pattern_filename.match( f ) ]
        for f in files :
            path = ufd+"/"+f
            writ = os.path.getmtime(path)
            writ_iso = datetime.datetime.fromtimestamp(writ).strftime('%Y-%m-%d %H:%M:%S')
            print("{:20} {}".format(path,writ_iso))
