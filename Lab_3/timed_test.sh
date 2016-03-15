#!/bin/bash


LIMIT=1000
for (( c=1; c<=$LIMIT; c++ )) 
do
   TIME=$(date +%s%N)
   ./a.out
   TIME=$(($(date +%s%N)-TIME))
   echo $TIME
done
