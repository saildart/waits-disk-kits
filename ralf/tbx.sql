# EXAMPLE tbx.sql
# ---------------------------------------------------------------------------------------------------
#       DASD_KIT / medium-combo
# ---------------------------------------------------------------------------------------------------
#       hand crafted by SAILDART human digital curators
#       related to the KITs now named "medium-people" "medium-IDE" "medium-exhibits"
#       combination of IDE-WAITS tools, 500 users names,
#       and cherry-picking for exhibit demonstrations.
# ---------------------------------------------------------------------------------------------------
# select the desired file names from the SAILDART archive
# to be placed into simulated SAIL-WAITS Ralph File System
#       as DASD.data8 one flat file of WAITS tracks,
#       as CKD disk packs, SIMH/SIM IBM-3330 200 MByte,
# and   as UCFS dual content, linux model of WAITS directories and files.
# --------------------------------------------------------------
# Usage:
#       sudo rm -f /var/tmp/ralf.csv
#       time $(cat fetch.sql | mysail -N)
#       real	0m21.411s
#
#       mv /var/tmp/ralf.csv ${YOUR_NEWEST_KIT_NAME}
# --------------------------------------------------------------
# select latest version [1,2] prior to 1975
# select latest version [3,2] prior to 1975
# select latest version [1,3] prior to 1975

set @yearmax=1975;

DROP TABLE if exists targ00;
CREATE TABLE targ00
SELECT unixname,max(v)version FROM mz
       WHERE left(dt,4)>= 1970
        and  left(dt,4)<= @yearmax
        and unixname rlike "^[23]/[13]/"
        and ext!="old" and ext!="msg" and ext!="pur"
        group by unixname;
        
REPLACE targ00 (unixname,version)
SELECT unixname,max(v)version FROM mz
        WHERE left(dt,4)>=1970
         and  left(dt,4)<= @yearmax
         and  unixname rlike "^SYS/(CSP|F40|J17|K17|LSP)/"
         and ext!="old" and ext!="msg" and ext!="pur"
         group by unixname;

REPLACE targ00 (unixname,version)
SELECT unixname,max(v)version FROM mz
        WHERE left(dt,4)>=1970
         and  left(dt,4)<= @yearmax
         and  ( unixname = 'blf/par/qparry'
             or unixname = 'kmc/dia/par2.fil'
         )
         group by unixname;

REPLACE targ00 (unixname,version)
SELECT unixname,max(v)version FROM mz
        WHERE left(dt,4)>=1970
         and  left(dt,4)<= @yearmax
         and ( prg rlike "^(ALS|LES|BGB|REG|DCS|SRS|LSP|DOC|FW|BH|TVR|JBR|MRC)$")
         group by unixname;

alter table targ00 add index (unixname);
alter table   mz   add index (unixname);

DROP TABLE if exists target;
CREATE TABLE target
     SELECT A.dt,A.prj,A.prg,A.filnam,A.ext,lpad(A.sn,6,'0')sn,A.wrdcnt,
          if(A.taxon rlike "TEXT","T",if(A.ext="DMP","X","B")) as tbx
       FROM mz A,targ00 B
       WHERE A.unixname=B.unixname and A.v=B.version;
       
DROP TABLE if exists targ00;
# -------------------------------------------------------------------------------
# Final version of plan '   prg'.PLN[2,2] files into target table. 552 such rows.
REPLACE target (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
SELECT  dt,prj,prg,filnam,ext,lpad(sn,6,'0'),wrdcnt,"T" FROM mz
 WHERE dartname rlike '^  2   2    [ A-Z][ A-Z][A-Z] PLN' and v=vn;
# --------------------------------------------------------------       
# Favorite versions
# sn=112243  1974-07-19 12:37    2/1/login.dmp
# sn=111534  1974-07-17 04:04    3/1/fail.dmp
# sn=116706  1974-08-25 00:25    3/1/e.dmp
# sn=111532  1974-07-17 19:57    3/1/logout.dmp
# sn=487167  1979-12-08 12:40    sys/spl/fact.txt
DELETE from target where (filnam='LOGIN' or filnam='LOGOUT') and (ext='DMP' or ext='OLD');
delete from target where sn=193045; # omit fail.dmp[1,3] 1975
REPLACE target (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
SELECT dt,prj,prg,filnam,ext,lpad(sn,6,'0')sn,wrdcnt,"X" FROM mz
WHERE sn=112243
   or sn=111532
   or sn=487167
   or sn=116706
   or sn=111534
   ;
# --------------------------------------------------------------
# Fix WAITS_617 source the hard way. Borrow COMCSS from K17 into J17. Edit OUTER fake 888888.
update target set sn=888888,wrdcnt=9446,dt='2021-05-13 17:02',tbx='T' where prg='sys' and prj='j17' and filnam='outer';
update target set sn=114948,wrdcnt=30080,dt='1974-08-15 18:33',tbx='T' where prg='sys' and prj='j17' and filnam='comcss';
# --------------------------------------------------------------       
# remove UGLY filenames and useless content#
delete from target where sn=232410 or sn=291416 or sn=689280;
delete from target where ext='xgp' or ext='old';
  drop table if exists sys0;
rename table target to sys0;

# Place the KEEPER file into each [1,prg] UFD for the about 500 users who left a plan file in [2,2]
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
select mx.dt,"1",mz.filnam,"KEEPER","",566684,10,"T" from mx,mz
where mx.sn=566684 and mz.prg='2' and mz.prj='2' and mz.ext='pln' and length(mz.filnam)<=3 group by mz.filnam;
# 382 rows affected.

# 49 versions of the FAIL assembler source
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
select dt,"FAI","SRC",concat("FAIL",lpad(v,2,"0"))filnam,"FAI",sn,wrdcnt,"T"
from mz where filnam='fail' and ext='' and prg='sys';

# 54 versions of the FAIL assembler DMP executible
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
select dt,"FAI","DMP",concat("FAIL",lpad(v,2,"0"))filnam,"DMP",sn,wrdcnt,"X"
from mz where filnam='fail' and ext='dmp' and prg='3' and prj='1';

# 45 versions of E.DMP
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
select dt,"E","DMP",concat("E",lpad(v,2,"0"))filnam,"DMP",sn,wrdcnt,"X" from mz where filnam='e' and ext='dmp' and prg='3' and prj='1' and left(dt,4)<=1975;

# 13 version of TECO.DMP
set @v=0;
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
select dt,"TEC","DMP",concat("TECO",lpad(@v:=@v+1,2,"0"))filnam,"DMP",sn,wrdcnt,"X" from mz where filnam='teco' and ext='dmp' and left(dt,4)<=1979;

# 6 version of TV.DMP
set @v=0;
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt,tbx)
select dt,"TV","DMP",concat("TV",lpad(@v:=@v+1,2,"0"))filnam,"DMP",sn,wrdcnt,"X" from mz where filnam='tv' and ext='dmp' and prj='1' and prg='3' and left(dt,4)<=1975;

# --------------------------------------------------------------       
#  FILE output field order:
#             &programmer,
#             &project,
#             &filename,
#             &extension,
#             &wrdcnt,
#             &tbx
#             &sn,
#             &year,&month,&day,&hour,&minute
#
select
lpad(prg,3,' '),
lpad(prj,3,' '),
rpad(filnam,6,' '),
rpad(ext,3,' '),
lpad(wrdcnt,8,' '),
tbx,
lpad(sn,6,'0'),
dt from sys0 order by prg,prj,filnam,ext
into OUTFILE '/var/tmp/ralf.csv' FIELDS TERMINATED BY ',' ENCLOSED BY '' LINES TERMINATED BY '\n';
# ---------------------------------------------------------------------------------------------------
# EOF.
