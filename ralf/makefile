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

SRC = main.d track.d word.d

main: ${SRC}
	@ls -l /KIT || (echo You need a symlink /KIT to point to a WAITS_DASD_KITS sub-directory && false)
	dmd -unittest -J. main.d

go: ${SRC}
	@ls -l /KIT || (echo You need a symlink /KIT to point to a WAITS_DASD_KITS sub-directory && false)
	dmd -J. -run main.d 1>/KIT/Ralf.log

# ----------------------------------------------------------------------------------------------------
# Input from /KIT/SYS.csv
#	seven comma delimited fields per line
#	PRG, PRJ, FILNAM, EXT, sn, wc, ISO_DATETIME
#	symbolic link /KIT/data8 must to point to a /data8 directory with the 900642 SAILDART sn files.
# Output goes into /KIT/DASD.data8
# ----------------------------------------------------------------------------------------------------

print:
	lpr -P Black1 -o cpi=11 -o lpi=7 -o page-left=36 -o landscape main.d track.d word.d
clean:
	rm -f *~ main main.o
purge:
	rm -f *~ main main.o /KIT/DASD.data8 /KIT/DASD.octal /KIT/Ralf.log
	rm -rf /KIT/track

# ----------------------------------------------------------------------------------------------------
# Fetch / select FROM SAILDART database
#
# Fetch:
#	sudo rm -f /var/tmp/SYS.csv
#	mysql -A saildart < fetch.sql
# ----------------------------------------------------------------------------------------------------
tracks:
	od -An -vto8 -w8 /KIT/DASD.data8 | cut -c12- > /KIT/DASD.octal
	rm -rf     /KIT/track
	mkdir -p   /KIT/track
	(cd /KIT/track;split --lines=2336 --numeric-suffixes --suffix-length=6 /KIT/DASD.octal)

# The default DASD sample consists of selected
# CSV lines from database mysail for the following 25. programmer codes
# ------------------------------------- #
#   2   3 SYS AIL DOC LSP BGB DEK	# Already marked as 'pub' available on the SAILDART
# REG  ME  BH DWP JAM RPH		# Gorin Frost Harvey Moorer Helliwell
# DAV DCS RFS HPM TES TVR		# Smith Swinehart Sproull Moravec Tesler Tovar
#   H  WD RWG				# Holloway Diffie Gosper
# JMC LES				# Principal Investigator and the Executive Director
# ------------------------------------- #
# EOF
