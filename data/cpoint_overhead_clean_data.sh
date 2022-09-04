#!/bin/bash

input=cpoint_overhead.txt
output=cpoint_overhead_clean.txt

base_line_avg=`awk 'BEGIN {sum=0; n=0} /baseline/ {sum+=$8; n+=1} END {print sum/n}' $input`
base_line_min=`awk '/baseline/ {print $8}' $input | sort | head -n 1`
base_line_max=`awk '/baseline/ {print $8}' $input | sort | tail -n 1`

softpm_avg=`awk 'BEGIN {sum=0; n=0} /softpm_create/ {sum+=$8; n+=1} END {print sum/n}' $input`
softpm_min=`awk '/softpm_create/ {print $8}' $input | sort | head -n 1`
softpm_max=`awk '/softpm_create/ {print $8}' $input | sort | tail -n 1`

consistent_avg=`awk 'BEGIN {sum=0; n=0} /softpm_consistent/ {sum+=$8; n+=1} END {print sum/n}' $input`
consistent_min=`awk '/softpm_consistent/ {print $8}' $input | sort | head -n 1`
consistent_max=`awk '/softpm_consistent/ {print $8}' $input | sort | tail -n 1`

echo -e "#Name\tMin\tAvg\tMax" > $output
echo -e "baseline\t${base_line_min}\t${base_line_avg}\t${base_line_max}" >> $output
echo -e "persistent\t${softpm_min}\t${softpm_avg}\t${softpm_max}" >> $output
echo -e "consistent\t${consistent_min}\t${consistent_avg}\t${consistent_max}" >> $output
