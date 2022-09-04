#!/bin/bash

input=restore_time.txt
output=restore_time_clean.txt

awk 'BEGIN {size=0} 
        /softpm_create_list/ {size=$4*4096/(1024*1024)} 
        /softpm_restore_list/ {print size, $8}' $input | \
    awk 'BEGIN {sum=0; c=0} // {sum+=$2; c+=1; if (c==10) {print $1, sum/c; sum=0; c=0}}' > $output

awk 'BEGIN {size=0} 
        /softpm_create_list/ {size=$4*4096/(1024*1024)} 
        /container0/ {print size, $5/(1024*1024)}' $input 
