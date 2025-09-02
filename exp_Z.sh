#!/bin/bash

P=1024
B=32
E=512
T=4
# Remember to specify a path for your dedicated storage device
DB_HOME="${RAM_DB_HOME:-./db_working_home}"
WORKLOAD_HOME="../workload_generator_scripts/"
Z_list=("1.0" "0.75" "0.5" "0.25" "0.1" "0.05" "0.01" "0.0")
ZD_list=("0" "1")
BCC=65536
bpk_list=("4.0" "6.0" "8.0")
OUTPUT_DIR="output-vary-Z"
mkdir -p ${OUTPUT_DIR}
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			echo "./bpk_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --BCC ${BCC} -b ${bpk} --dd --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_query_workload.txt --dqs --query-stats-path ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_query_stats.txt  --run-stats-op ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_bpk-${bpk}_output.txt"
			./bpk_benchmark -P${P} -T${T} -E ${E} -B ${B} -p ${DB_HOME} -V 1 --BCC ${BCC} -b ${bpk} --dd --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_query_workload.txt --dqs --query-stats-path ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_query_stats.txt --run-stats-op ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_bpk-${bpk}_output.txt
			rm ${DB_HOME}-monkey/*
			rm ${DB_HOME}-monkey-plus/*
			rm ${DB_HOME}-optimal/*
		done
		#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./output/
		cd ${OUTPUT_DIR}
		../output/grep_accessed_data_blocks_by_bpk_microbenchmark.sh ${ZD} ${Z} > data_blocks_ZD${ZD}_Z${Z}_result.txt
		../output/grep_query_latency_by_bpk_microbenchmark.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt
		cd -
	done
	for bpk in ${bpk_list[@]}
	do
		cd ${OUTPUT_DIR}
		../output/grep_accessed_data_blocks_by_Z_microbenchmark.sh ${ZD} ${bpk} > data_blocks_ZD${ZD}_bpk-${bpk}_result.txt
		../output/grep_query_latency_by_Z_microbenchmark.sh ${ZD} ${bpk} > query_latency_ZD${ZD}_bpk-${bpk}_result.txt
		cd -
	done
	
done
