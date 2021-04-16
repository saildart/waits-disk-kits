# RALF - Ralph Gorin File System DASD.data8 Maker

This sub-directory contains a 'D' language program (an outlier in my œuvre)
which can do the heavy lifting of building large (and even small) DASD.data8 files
that comprise all the tracks (the block level structure)
of a SAIL WAITS IBM-3330 shared file system, excluding the UDP pack(s).

The list of file names is specified by a database table file, ralf.csv,
which refers to a 54G file system at mount point /data8/sn.

Each /data8/sn/xxxxxx serial numbered SAILDART archive blob
contains the content of one (or sometimes many) SAIL file names.









