# README waits-disk-kits

# Synopsis

At present, April 2021, this repository is a parking lot for re-factoring
the SAILDART storage management software that generates waits-disk file-system images for simulators.
I have promised myself two (or more) usable WAITS disk-image kits by the end of this month, April 2021.
- BgBaumgart.

<!-- # Shortcuts -->

# RALF - Ralph Gorin WAITS File System Model

See the README file with no extension.
RALF requires 'D' language tools and curator access to a saildart archive VAULT mount point.
The recent RALF readme file was addressed to Lars Brinkhoff, 3 March 2021,
and is based on my saildart.org site work from 2014.
RALF might be useful as documentation for people who read 'D' code.
The original PDP-10 code basis for RALF
is at https://www.saildart.org/[J18,SYS]/
and the RALPH program source is in the directory at https://www.saildart.org/[ACT,REG]/
- amen. BgBaumgart April 2021.

<!--
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

# Tracks - the term for the SAIL-WAITS notion of disk block
-->

# CKD _Chickadee_ disk pack files

The TLA (Three-Letter-Acronym) CKD stands for (Count, Key, Data) which is how
IBM (International Business Machines) stored DASD (Direct-Access Storage Device) data.

# SYS ambiguity

The TLA (Three-Letter-Acronym) **SYS** has been overused and has now (2021) become useless.
The term **SYS** is always context dependent.

* SYS inside WAITS-1974 at montior command level is the [1,3] directory.
* SYS{number} for Baumgart javascript is a pathname for the full file system as octal tracks.
* SYS{number} for Cornwell Github SIM is for CKD formatted individual disk packs.

The generic path SYS is replaced by pathnames DASD, PACK, TRACK, KIT and UCFS.
The total content of the SAILDART archive moves from its TimeCapsule
thru three "work-room" containers named OUTPUT, BASEMENT and VAULT
and is exhibited on it web site from mount point named /Large2012.

# Disk Kits

The Frankenstein-monster UPPER-CASE File-System, at pathname **KIT/UCFS**,
is formed by sewing together old and new parts that Igor finds within a particular KIT's work rooms
after the curators, simulators, human hackers and visitors have re-arranged previously collected
specimens or have fabricated new items using the available emulators, simulators, processors
and stormy lightning.

The hermaphroditic surgical tools allow gender changing the original DART data between
binary, data8, data9, text, octal, SIMH ckd, tracks, diskpacks, csv, jason, pdf, yaml
and whatever is needed each year.

Kit pathname suggestions
* the five name prefixes shall be: tiny, small, medium, large, huge
* the name postfix should be mnemonic or metaphorical;
  NOT a forgettable sequential numeric or creation date;
  which do not help in remembering the content of the kit or its intended use or user audience.

## Existing Kits

kit name      | date made  |  prg |  ppn | files |  DMP | description
------------- | ---------- | ---- | ---- | ----- | -----|-----------------------------------------------------
medium-people | 2020-01-18 |  520 |  644 |  7243 |  734 | benchmark adequate 1974 with many people named
tiny-IDE      | 2021-04... |  ... |  ... |  .... |  ... | minimal system with load-and-assembly tools for 1974

## Potential Future Kits

kit name             | description
-------------------- | --------------------------------------------------
tiny-DEC-diagnostics | MAIN DEC KA whatever
small-BGB            | Baumgart
small-LES            | Earnest
SMALL-JMC            | John McCarthy
small-REG            | Ralph Gorin
medium-LISP          |
medium-TEX           |
medium-MUSIC         |
large-CSP-19??       | SAIL WAITS systems software for a given year
huge-all             | all version2 of every file 1972 to 1990

