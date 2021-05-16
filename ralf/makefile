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

/usr/local/bin/ralf: ${SRC}
	dmd -of/usr/local/bin/ralf -J. main.d

unit: ${SRC}
	dmd -unittest -J. main.d

go:
	ralf $$KIT

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
	rm -f *~ main main.o ./KIT/DASD.data8 ./KIT/DASD.octal ./KIT/ralf.log
	rm -rf ./KIT/track

# ----------------------------------------------------------------------------------------------------
# Fetch / select FROM SAILDART database
#
# Fetch:
#	sudo rm -f /var/tmp/SYS.csv
#	mysql -A saildart < fetch.sql
# ----------------------------------------------------------------------------------------------------
tracks:
	od -An -vto8 -w8 $$KIT/DASD.data8 | cut -c12- > $$KIT/DASD.octal
	rm -rf     $$KIT/track
	mkdir -p   $$KIT/track
	(cd $$KIT/track;split --lines=2336 --numeric-suffixes --suffix-length=6 ../DASD.octal)
	cd $$KIT/track
#       for x in *;do awk '{printf("%s %s",$1,(NR%8==0 ? "\n":""));}' $$x >tmp;mv tmp $$x;done
#
# awk '{printf("%s %s",$1,(NR%8==0 ? "\n":""));}' $KIT/track/x00000
#

# One DASD sample consisted of selected
# CSV lines from database mysail for the following 25. programmer codes
# ------------------------------------- #
#   2   3 SYS AIL DOC LSP BGB DEK	# Already marked as 'pub' available on the SAILDART
# REG  ME  BH DWP JAM RPH		# Gorin Frost Harvey Moorer Helliwell
# DAV DCS RFS HPM TES TVR		# Smith Swinehart Sproull Moravec Tesler Tovar
#   H  WD RWG				# Holloway Diffie Gosper
# JMC LES				# Principal Investigator and the Executive Director
# ------------------------------------- #
fetch:
	@echo ";\n; EXAMPLE commands to fetch a table of filenames . . .\n;\n\
	sudo rm -f /var/tmp/ralf.csv\n\
	time(cat fetch.sql | mysail -N)\n\
	wc -l /var/tmp/ralf.csv\n\
	cp /var/tmp/ralf.csv YOUR_KIT\n\
	cp /var/tmp/ralf.csv $$KIT\n"
# ------------------------------------- #
# misc make targets
tbx:
	sudo rm -f /var/tmp/ralf.csv
	mysql --defaults-file=/home/bgb/.my.cnf.saildart < tbx.sql
# ------------------------------------- #
# EOF
