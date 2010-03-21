#!/bin/bash

for l in 2 3 4 5 6 7 8
do
  for d in 6 8 10 12 15 20 25
  do 
     for p in 2 2.5 3 3.5 4 5
     do
	   echo 3 > /proc/sys/vm/drop_caches
       ./count 3 /home/wwang/fsgen/49731/ 84373 895924 0 $l $d $p
     done
   done
done
#!/bin/bash

