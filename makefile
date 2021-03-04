# makefile
# -------------------------------------------------------------------------------- #
# Make a binary SAIL disk image for SYSTEM.DMP[J17,SYS] that is
# compatible with the KA10 operating system of 26 July 1974.
#
# This may include documentation, rearch notes, plans,
# data bases and personal file areas that were created after 1974.
#
# The disk space is implemented with 64-bit words for each 36-bit word.
# The extra 28 bites per word are zero.
# -------------------------------------------------------------------------------- #

SRC = main.d

main: ${SRC}
	dmd -unittest -J. main.d

go: ${SRC}
	mkdir -p large/SYS
	dmd -J. -run main.d 1>large/SYS/Ralf.log
# Input		from	large/SYS.csv
#	seven comma delimited fields per line
#	PRG,PRJ,FILNAM,EXT,sn,wc,ISO_DATETIME
#   note symbolic link  large/data8	needs to point at a /data8 directory with the 900642 SAILDART sn files.
#
# Output	into	large/DISK1974
#	Data8 disk image
# print:
#	lpr -P Black1 -o cpi=11 -o lpi=7 -o page-left=36 -o landscape main.d

clean:
	rm -f *~ main large/DISK.data8 large/DISK.octal
	rm -rf large/SYS
# ----------------------------------------------------------------------------------------------------
# Fetch / select FROM SAILDART database
#
# fetch:
#	sudo rm -f /var/tmp/SYS.csv
#	mysql -A saildart < fetch.sql
# ----------------------------------------------------------------------------------------------------
tracks:
	@echo This might take as long as TEN minutes:
	od -An -vto8 -w8 large/DISK.data8 | cut -c12- > large/DISK.octal
	rm -rf     large/SYS/track
	mkdir -p   large/SYS/track
	(cd large/SYS/track;split --lines=2336 --numeric-suffixes --suffix-length=6 ../../DISK.octal)
# tracks0:
#	echo rsync -av /large/SYS0/ serv1:/data/SYS0/
#	time rsync -av /large/SYS0/ serv1:/data/SYS0/
#	@echo This may take as long as EIGHT minutes...
#	ssh serv1 '(od -An -vto8 -w8 /data/SYS0/DISK.data8 | cut -c12- > /data/SYS0/DISK.octal)'
#	ssh serv1 'rm -rf /data/SYS0/track'
#	mkdir -p   /data/SYS0/track
#	ssh serv1 '(cd /data/SYS0/track;split --lines=2336 --numeric-suffixes --suffix-length=6 /data/SYS0/DISK.octal)'
# ----------------------------------------------------------------------------------------------------
# Binary 8-bytes per PDP10 word into OCTAL-text 13-characters-per-word.
# 12 octal digits and a NEWLINE per PDP10 word, takes about 40 seconds.
# -An disable Address Field
# into13:
#	od -An -vto8 -w8 DISK1974|cut -c12- > /data/13/track/DISK1974
#
# Binary 8-bytes per PDP10 word into HEX-text 10-characters-per-word.
# 9 hex digits and a NEWLINE per PDP10 word, takes about 40 seconds.
# into10:
#	od -vtx8 -w8 DISK1974|cut -c16- > /data/10/DISK1974
#
# DISK1974 sample consists of selected
# CSV lines from mysail for the following 25. programmer codes
# ------------------------------------- #
#   2   3 SYS AIL DOC LSP BGB DEK	# Already marked as 'pub' available on the SAILDART
# REG  ME  BH DWP JAM RPH		# Gorin Frost Harvey Moorer Helliwell
# DAV DCS RFS HPM TES TVR		# Smith Swinehart Sproull Moravec Tesler Tovar
#   H  WD RWG				# Holloway Diffie Gosper
# JMC LES				# Principal Investigator and the Executive Director
# ------------------------------------- #
# This fake MRESTORE generates the [1,1] directory area consisting of the *.UFD files
# one for each [ project, programmer ] directory, 215. in the first sample.
#
# I omit the NS news service files with their ugly filenames containing punctuation characters.
# Also Marty Frost had wanted to be the archival curator for the NS news service.
# ---
# EOF
