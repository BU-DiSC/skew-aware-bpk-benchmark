#!/bin/bash
ZD=$1
bpk=$2
str="ZD${ZD}_bpk-${bpk}"
echo uniform
grep "accessed data blocks:" *output.txt | grep ${str} | awk -F':|_|Z' '{print $2*100";"$NF}' | awk -F';' '{print $1, $NF}' | sort -g | awk '{print $2}'
#grep "accessed data blocks:" *output.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
echo monkey
grep "accessed data blocks (monkey):" *output.txt | grep ${str} | awk -F':|_|Z' '{print $2*100";"$NF}' | awk -F';' '{print $1, $NF}' | sort -g | awk '{print $2}'
echo "monkey+"
grep "accessed data blocks (monkey+):" *output.txt | grep ${str} | awk -F':|_|Z' '{print $2*100";"$NF}' | awk -F';' '{print $1, $NF}' | sort -g | awk '{print $2}'
echo "optimal"
grep "accessed data blocks (optimal):" *output.txt | grep ${str} | awk -F':|_|Z' '{print $2*100";"$NF}' | awk -F';' '{print $1, $NF}' | sort -g | awk '{print $2}'
