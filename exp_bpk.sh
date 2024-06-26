#!/bin/bash

P=512
B=16
T=4
# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="./db_working_home"
Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "3")
#Z_list=("0.0")
#ZD_list=("3")
BCC=20480
bpk_list=("1.0" "1.5" "2.0" "2.5" "3.0" "3.5" "4.0" "4.5" "5.0" "5.5" "6.0" "6.5" "7.0" "7.5" "8.0" "8.5" "9.0" "9.5" "10.0" "10.5" "11.0")
bpk_list=("2.0" "3.0" "4.0" "5.0" "6.0" "7.0" "8.0")
#bpk_list=("1.0" "9.0" "12.0")
mkdir -p "output/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			echo "./bpk_benchmark -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC ${BCC} -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --dr"
			./bpk_benchmark -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct -V 1 --BCC ${BCC} -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --dr --dqs --query-stats-path output/Z${Z}_ZD${ZD}_query_stats.txt > output/Z${Z}_ZD${ZD}_bpk-${bpk}_output.txt
			rm ${DB_HOME}/*
			rm ${DB_HOME}-monkey/*
			rm ${DB_HOME}-monkey-plus/*
			rm ${DB_HOME}-optimal/*
	
		done
		#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./output/
		cd output/
		./grep_accessed_data_blocks_by_bpk_microbenchmark.sh ${ZD} ${Z} > data_blocks_ZD${ZD}_Z${Z}_result.txt
		./grep_query_latency_by_bpk_microbenchmark.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt
		cd -
	done
done
cp ../workload_generator_scripts/ingestion_workload.txt output/
