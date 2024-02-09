#!/bin/bash
ZD=$1
Z=$2
str="Z${Z}_ZD${ZD}_bpk"
echo uniform
grep "point query latency:" *output.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
echo monkey
grep "point query latency (monkey):" *output.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
echo "monkey+"
grep "point query latency (monkey+):" *output.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
echo "optimal"
grep "point query latency (optimal):" *output.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
