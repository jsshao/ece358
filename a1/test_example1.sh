#!/bin/bash
EXECS=(addpeer removepeer addcontent removecontent lookupcontent)

# unzip
unzip -o a1-358s16

# make
make clean ${EXECS[*]}

# check for existence
for i in ${EXECS[*]};
do
  if [ ! -f $i ]; then
    echo "ERROR: \"$i\" not found "
    exit 1
  fi
done

# sample test
set -x
TMPFILE=temp`date +%H%M%S%N`.txt

# ---- adding peers ----
./addpeer > $TMPFILE
PEER1=`sed -n '1p' $TMPFILE`

# ---- adding content ----
./addcontent $PEER1 'hello world' >> $TMPFILE
C1KEY=`sed -n '2p' $TMPFILE`

# ---- get content ----
./lookupcontent $PEER1 $C1KEY >> $TMPFILE

# ---- rm content ----
./removecontent $PEER1 $C1KEY

# ---- rm peers ----
./removepeer $PEER1

# ---- TMPFILE ----
cat $TMPFILE
