#!/bin/bash

P=512
B=64
T=4
bpk=6

Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "3")

mkdir -p "output/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		echo "./bpk_benchmark -P${P} -T${T} -B ${B} --no_dynamic_cmpct --BCC 524288 -b ${bpk} --dd --iwp /scratchHDDa/zczhu/K-V-Workload-Generator/ingestion_workload.txt --qwp /scratchHDDa/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt --dqs"
		./bpk_benchmark -P${P} -T${T} -B ${B} --no_dynamic_cmpct -V 1 --BCC 524288 -b ${bpk} --dd --iwp /scratchHDDa/zczhu/K-V-Workload-Generator/ingestion_workload.txt --qwp /scratchHDDa/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt --dqs --query-stats-path output/Z${Z}_ZD${ZD}_query_stats.txt > output/Z${Z}_ZD${ZD}_output.txt
		rm db_working_home/*
		rm db_working_home-monkey/*
		rm db_working_home-monkey-plus/*
		rm db_working_home-optimal/*
		cp /scratchHDDa/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt ./output/
	done
	cd output/
	./grep_accessed_data_blocks.sh ${ZD} > data_blocks_ZD${ZD}_result.txt
	cd -
done
cp /scratchHDDa/zczhu/K-V-Workload-Generator/ingestion_workload.txt output/
