#!/bin/bash
ZD=$1
str="ZD${ZD}"
echo uniform
grep "accessed data blocks:" *output.txt | grep ${str} | awk -F':' '{print $3}'
echo monkey
grep "accessed data blocks (monkey):" *output.txt | grep ${str} | awk -F':' '{print $3}'
#echo "monkey+"
#grep "accessed data blocks (monkey+):" *output.txt | grep ${str} | awk -F':' '{print $3}'
echo "optimal"
grep "accessed data blocks (workloadaware):" *output.txt | grep ${str} | awk -F':' '{print $3}'
