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

## Source files

        main.d          # ralfs build disk images, from /data8/sn/###### files.
          word.d        # PDP10 word
          track.d       # SAT and RIB of Stanford File System
        makefile  
        fetch.sql       # example fetch from DATA base
        SYS.csv         # example list of SAIL files to fetch from data8/sn/######

# Output path names

# tracks - the term for the SAIL-WAITS notion of disk block

# CKD _Chickadee_ files

The TLA (Three-Letter-Acronym) CKD stands for (Count, Key, Data) which is how
IBM (International Business Machines) stored DASD (Direct-Access Storage Device) data.


# SYS ambiguity

So too the TLA **SYS** has been overused and is so is now almost useless.

* SYS inside WAITS-1974 at montior command level is the [1,3] directory.
* SYS for Baumgart javascript is a model for the full file system.
* SYS for Cornwell Github SIM is for individual disk packs.

# Hermaphrodite

A Frankenstein-monster file-system formed by sewing together all the old and new parts
that Igor has found around the Saildart Archive.

# named SYS: file sets

name              | date made  |  prg |  ppn | files |  DMP | description
----------------- | ---------- | ---- | ---- | ----- | -----|-----------------------------------------------
SYS-medium-people | 2021-03-03 |  520 |  644 |  7243 |  734 | benchmark adequate 1974 with many people named
SYS-tiny-IDE      | 2021-04... |  ... |  ... |  .... |  ... | minimal system load and assembly tools
