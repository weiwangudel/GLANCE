#!/bin/bash

for i in 32 1515 5537 49731 64141
do

   if [ $i -eq 32 ] ; then
	dcount=1013
        fcount=27864
   else if [ $i -eq 1515 ] ; then
	dcount=14608
	fcount=99626
   else if [ $i -eq 5537 ] ; then
        dcount=1241
        fcount=27132
   else if [ $i -eq 49731 ] ; then
        dcount=84373
        fcount=895924
   else if [ $i -eq 64141 ] ; then
        dcount=97169
        fcount=1055126
   fi
   fi
   fi
   fi
   fi

for l in 2 3 4 5 6 7 8
do
  for d in 6 8 10 12 15 20 25
  do
     for p in 2 2.5 3 3.5 4 5
     do
           echo 3 > /proc/sys/vm/drop_caches
       ./count 5 /home/wwang/fsgen/$i/ $dcount $fcount  0 $l $d $p >> $i.txt
     done
   done
done

done
