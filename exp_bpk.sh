#!/bin/bash

P=512
B=16
T=4
DB_HOME="/mnt/ramd/zczhu/db_testing_home"
Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "3")
Z_list=("0.0")
ZD_list=("3")
bpk_list=("1.0" "1.5" "2.0" "2.5" "3.0" "3.5" "4.0" "4.5" "5.0" "5.5" "6.0" "6.5" "7.0" "7.5" "8.0" "8.5" "9.0" "9.5" "10.0" "10.5" "11.0")
bpk_list=("1.0" "9.0" "12.0")
mkdir -p "output/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			echo "./bpk_benchmark -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 524288 -b ${bpk} --dd --iwp /scratchHDDa/zczhu/K-V-Workload-Generator/ingestion_workload.txt --qwp /scratchHDDa/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt --dqs"
			./bpk_benchmark -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct -V 1 --BCC 524288 -b ${bpk} --dd --iwp /scratchHDDa/zczhu/K-V-Workload-Generator/ingestion_workload.txt --qwp /scratchHDDa/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt --dqs --query-stats-path output/Z${Z}_ZD${ZD}_query_stats.txt > output/Z${Z}_ZD${ZD}_bpk-${bpk}_output.txt
			rm ${DB_HOME}/*
			rm ${DB_HOME}-monkey/*
			rm ${DB_HOME}-monkey-plus/*
			rm ${DB_HOME}-optimal/*
	
		done
		cp /scratchHDDa/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt ./output/
		cd output/
		./grep_accessed_data_blocks_by_bpk.sh ${ZD} ${Z} > data_blocks_ZD${ZD}_Z${Z}_result.txt
		cd -
	done
done
cp /scratchHDDa/zczhu/K-V-Workload-Generator/ingestion_workload.txt output/
