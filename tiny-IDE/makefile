# makefile for disk kit named "tiny-IDE"
SHELL=/bin/bash
all:
	@mkdir -p backup ralf maud
	@echo This is tiny-IDE. PWD is $$PWD
	realpath .
build:
	test -a DASD.data8 && mv --backup=t DASD.data8 backup || true
	ralf .
	test -a 0.ckd && mv --backup=t 0.ckd backup || true
	DASD_into_chickadee .
upper:
	test -a UCFS && mv --backup=t UCFS backup || true
	chickadee_into_UCFS .
rebuild:
	test -a DASD.data8 && mv --backup=t DASD.data8 backup || true
	maud .
	test -a 0.ckd && mv --backup=t 0.ckd backup || true
	DASD_into_chickadee .
clean:
	rm -rf UCFS
	rm -rf track
	rm -f DASD.data8 DASD.octal
	rm -f 0.ckd ckd.*
	rm -f *~
purge:
	make clean
	rm -rf maud ralf backup
backup:
	mkdir -p backup
	mv --backup=t UCFS DASD.data8 0.ckd backup
tracks:
	od -An -vto8 -w8 DASD.data8 | cut -c12- > DASD.octal
	rm -rf     track
	mkdir -p   track
	(cd track;split --lines=2336 --numeric-suffixes --suffix-length=6 ../DASD.octal)
	cd track
ralfrun:
	rm -f DASD.data8 DASD.octal tracks 0.ckd UCFS
	ralf .
	make tracks
	DASD_into_chickadee .
	chickadee_into_UCFS .
maudrun:
	rm -f DASD.data8 DASD.octal tracks 0.ckd
	maud . > maud.log
	make tracks
	DASD_into_chickadee .
up:
	rm -rf UCFS
	chickadee_into_UCFS .
setralf:
	@mkdir -p ralf
	mv DASD.data8	DASD.octal track  ralf/
setmaud:
	@mkdir -p maud
	mv DASD.data8	DASD.octal track  maud/

