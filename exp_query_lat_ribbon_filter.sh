#!/bin/bash

P=1024
B=32
T=4
E=512
R=1
bpk_list=("2" "3" "4" "5" "6" "7")
BCC="524288"
# Remember to specify a path for your dedicated storage device
DB_HOME="${SLOW_DB_HOME:-./db_working_home}"
WORKLOAD_HOME="../workload_generator_scripts"
Z_list=("1.0")
ZD_list=("0")
OUTPUT_DIR="output-bpk-ribbon-filter"
mkdir -p "${OUTPUT_DIR}"
#ulimit -n 65535
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			
			echo "./query_lat_exp -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_mixed_update_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dr --rf --run-stats-op ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_bpk-${bpk}-rf-output.txt"
			./query_lat_exp -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_mixed_update_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dr --rf --run-stats-op ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_bpk-${bpk}-rf-output.txt
			rm ${DB_HOME}-mnemosyne/*
			rm ${DB_HOME}-mnemosyne-plus/*
		done
		cd ${OUTPUT_DIR}/
		echo "../output/grep_accessed_data_blocks_by_bpk.sh ${ZD} ${Z} > accessed_data_blocks_ZD${ZD}_Z${Z}_result.txt"
		echo "../output/grep_read_bytes_by_bpk.sh ${ZD} ${Z} > read_bytes_ZD${ZD}_Z${Z}_result.txt"
		echo "../output/grep_query_latency_by_bpk.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt"
		echo "../output/grep_avg_latency_by_bpk.sh ${ZD} ${Z} > avg_latency_ZD${ZD}_Z${Z}_result.txt"
		echo "../output/grep_write_latency_by_bpk.sh ${ZD} ${Z} > write_latency_ZD${ZD}_Z${Z}_result.txt"
		../output/grep_accessed_data_blocks_by_bpk.sh ${ZD} ${Z} > accessed_data_blocks_ZD${ZD}_Z${Z}_result.txt
		../output/grep_read_bytes_by_bpk.sh ${ZD} ${Z} > read_bytes_ZD${ZD}_Z${Z}_result.txt
		../output/grep_query_latency_by_bpk.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt
		../output/grep_avg_latency_by_bpk.sh ${ZD} ${Z} > avg_latency_ZD${ZD}_Z${Z}_result.txt
		../output/grep_write_latency_by_bpk.sh ${ZD} ${Z} > write_latency_ZD${ZD}_Z${Z}_result.txt
		cd -

	done
done
#cp ../workload_generator_scripts/ingestion_workload.txt output/
