#!/bin/bash

for i in 32 49731 1515 64141
do
  ./run_$i.sh > run_$i.txt
done
