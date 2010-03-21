#!/bin/bash

for l in 5 #4 5 6 7
do
  for d in 5 #10 15 20
  do 
     for p in 2 #3 4 5
     do
	   echo 3 > /proc/sys/vm/drop_caches
       ./count 1 /home/wwang/fsgen/32/ 1013 27864 0 $l $d $p
     done
   done
done
