# EXAMPLE fetch.sql
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
         and ( prg rlike "^(BGB|REG|LSP|DOC)$")
         group by unixname;

alter table targ00 add index (unixname);
alter table   mz   add index (unixname);

DROP TABLE if exists target;
CREATE TABLE target
     SELECT A.dt,A.prj,A.prg,A.filnam,A.ext,lpad(A.sn,6,'0')sn,A.wrdcnt
       FROM mz A,targ00 B
       WHERE A.unixname=B.unixname and A.v=B.version;
       
DROP TABLE if exists targ00;
# -------------------------------------------------------------------------------
# Final version of plan '   prg'.PLN[2,2] files into target table. 552 such rows.
REPLACE target (dt,prj,prg,filnam,ext,sn,wrdcnt)
SELECT  dt,prj,prg,filnam,ext,lpad(sn,6,'0'),wrdcnt FROM mz
 WHERE dartname rlike '^  2   2    [ A-Z][ A-Z][A-Z] PLN' and v=vn;
# --------------------------------------------------------------       
# Favorite versions
# sn=112243  1974-07-19 12:37    2/1/login.dmp
# sn=111532  1974-07-17 19:57    3/1/logout.dmp
# sn=487167  1979-12-08 12:40    sys/spl/fact.txt
DELETE from target where (filnam='LOGIN' or filnam='LOGOUT') and (ext='DMP' or ext='OLD');
REPLACE target (dt,prj,prg,filnam,ext,sn,wrdcnt)
SELECT dt,prj,prg,filnam,ext,lpad(sn,6,'0')sn,wrdcnt FROM mz
WHERE sn=112243
   or sn=111532
   or sn=487167;
# --------------------------------------------------------------       
# remove UGLY filenames and useless content#
delete from target where sn=232410 or sn=291416 or sn=689280;
delete from target where ext='xgp' or ext='old';
  drop table if exists sys0;
rename table target to sys0;

# Place the KEEPER file into each [1,prg] UFD for the about 500 users who left a plan file in [2,2]
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt)
select mx.dt,"1",mz.filnam,"KEEPER","",566684,10 from mx,mz
where mx.sn=566684 and mz.prg='2' and mz.prj='2' and mz.ext='pln' and length(mz.filnam)<=3 group by mz.filnam;
# 382 rows affected.

# 49 versions of the FAIL assembler source
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt)
select dt,"FAI","SRC",concat("FAIL",lpad(v,2,"0"))filnam,"FAI",sn,wrdcnt
from mz where filnam='fail' and ext='' and prg='sys';

# 54 versions of the FAIL assembler DMP executible
insert sys0 (dt,prj,prg,filnam,ext,sn,wrdcnt)
select dt,"FAI","DMP",concat("FAIL",lpad(v,2,"0"))filnam,"DMP",sn,wrdcnt
from mz where filnam='fail' and ext='dmp' and prg='3' and prj='1';

# --------------------------------------------------------------       
#  FILE output field order:
#             &programmer,
#             &project,
#             &filename,
#             &extension,
#             &sn, &wrdcnt,
#             &year,&month,&day,&hour,&minute
#
select prg,prj,filnam,ext,lpad(sn,6,'0'),wrdcnt,dt from sys0 order by prg,prj,filnam,ext
into OUTFILE '/var/tmp/ralf.csv' FIELDS TERMINATED BY ',' ENCLOSED BY '' LINES TERMINATED BY '\n';
# ---------------------------------------------------------------------------------------------------
# EOF.
