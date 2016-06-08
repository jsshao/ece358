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

# we will add our tests after this point
