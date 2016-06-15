#!/bin/bash
EXECS=(addpeer removepeer addcontent removecontent lookupcontent)

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
TMPFILE=temp`date +%H%M%S%N`.txt

# ---- adding peers ----
./addpeer > $TMPFILE
PEER1=`sed -n '1p' $TMPFILE`

# ---- adding content ----
./addcontent $PEER1 'hello world' >> $TMPFILE
C1KEY=`sed -n '2p' $TMPFILE`
./addpeer $PEER1 >> $TMPFILE
./addpeer $PEER1
./addpeer $PEER1
./addpeer $PEER1
./addpeer $PEER1
./addpeer $PEER1
for i in `seq 1 100`;
do
    echo $i
    ./addpeer $PEER1 > jason
    PEER=`sed -n '1p' jason`
    ./removepeer $PEER
done  
./addpeer $PEER1

# ---- get content ----
./lookupcontent $PEER1 $C1KEY >> $TMPFILE

# ---- rm content ----
./removecontent $PEER1 $C1KEY

# ---- rm peers ----
./removepeer $PEER1

# ---- TMPFILE ----
cat $TMPFILE
