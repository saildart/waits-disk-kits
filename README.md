# README waits-disk

# Synopsis

At present, April 2021, this repository is a parking lot for re-factoring
the SAILDART software that generates waits-disk file-system images.

# Shortcuts

# RALF - Models of the Ralph Gorin File System built from mysql MFD tables.

  The drill is like this :
        make
        make go
        make tracks
        make clean

# Source files

        main.d          # ralfs build disk images, from /data8/sn/###### files.
          word.d        # PDP10 word
          track.d       # SAT and RIB of Stanford File System
        makefile  
        fetch.sql       # example fetch from DATA base
        SYS.csv         # example list of SAIL files to fetch from data8/sn/######

# Output path names

# tracks - the term for the SAIL-WAITS notion of disk block

# CKD _Chickadee_ files

The TLA (Three-Letter-Acronym)
CKD (Count, Key, Data) which is how
IBM (International Business Machines) stored
DASD (Direct-Access Storage Device) data.

# Hermaphrodite

A Frankenstein-monster file-system
formed by sewing together old and new parts
that Igor has found around the Archive.
