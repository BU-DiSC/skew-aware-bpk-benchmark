#!/bin/bash
ZD=$1
Z=$2
str="Z${Z}_ZD${ZD}_bpk"
echo uniform
grep "ingestion latency:" *output_simple_mixed_workload.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
echo monkey
grep "ingestion latency (monkey):" *output_simple_mixed_workload.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
echo "workloadaware"
grep "ingestion latency (workloadaware):" *output_simple_mixed_workload.txt | grep ${str} | awk -F':|-' '{print $2";"$4}' | awk -F'_|;' '{print $1,$NF}' | sort -g | awk '{print $2}'
