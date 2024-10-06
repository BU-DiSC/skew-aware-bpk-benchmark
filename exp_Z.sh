#!/bin/bash

P=1024
B=32
E=512
T=4
# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="/mnt/ramd/zczhu/db_working_home"
WORKLOAD_HOME="/scratchHDDa/zczhu/dynamic_bpk_emulation/workload_generator_scripts/"
#WORKLOAD_HOME="/scratchHDDb/zczhu/K-V-Workload-Generator/"
#DB_HOME="./db_working_home"
Z_list=("1.0" "0.75" "0.5" "0.25" "0.1" "0.05" "0.01" "0.0")
ZD_list=("0" "1")
NDEV="5.0"
NMP="0.95"
#Z_list=("0.0")
#ZD_list=("3")
BCC=65536
bpk_list=("1.0" "1.5" "2.0" "2.5" "3.0" "3.5" "4.0" "4.5" "5.0" "5.5" "6.0" "6.5" "7.0" "7.5" "8.0" "8.5" "9.0" "9.5" "10.0" "10.5" "11.0")
bpk_list=("2.0" "3.0" "4.0" "5.0" "6.0" "7.0" "8.0" "9.0" "10.0" "11.0")
bpk_list=("4.0" "6.0" "8.0")
mkdir -p "output-bpk/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			#echo "./bpk_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --BCC ${BCC} -b ${bpk} --dd --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt"
			#./bpk_benchmark -P${P} -T${T} -E ${E} -B ${B} -p ${DB_HOME} -V 1 --BCC ${BCC} -b ${bpk} --dd --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt --dqs --query-stats-path output-bpk/Z${Z}_ZD${ZD}_query_stats.txt > output-bpk/Z${Z}_ZD${ZD}_NDEV${NDEV}_bpk-${bpk}_output.txt
			echo "./bpk_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --BCC ${BCC} -b ${bpk} --dd --dr --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt"
			./bpk_benchmark -P${P} -T${T} -E ${E} -B ${B} -p ${DB_HOME} -V 1 --BCC ${BCC} -b ${bpk} --dd --dr --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt --dqs --query-stats-path output-bpk/Z${Z}_ZD${ZD}_query_stats.txt > output-bpk/Z${Z}_ZD${ZD}_NDEV${NDEV}_bpk-${bpk}_output.txt
			rm ${DB_HOME}/*
			rm ${DB_HOME}-monkey/*
			rm ${DB_HOME}-monkey-plus/*
			rm ${DB_HOME}-optimal/*
			#echo "./bpk_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --BCC ${BCC} -b ${bpk} --dd --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_query_workload.txt"
			#./bpk_benchmark -P${P} -T${T} -E ${E} -B ${B} -p ${DB_HOME} -V 1 --BCC ${BCC} -b ${bpk} --dd --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_query_workload.txt --dqs --query-stats-path output-bpk/Z${Z}_ZD${ZD}_query_stats.txt > output-bpk/Z${Z}_ZD${ZD}_bpk-${bpk}_output.txt
			#rm ${DB_HOME}/*
			#rm ${DB_HOME}-monkey/*
			#rm ${DB_HOME}-monkey-plus/*
			#rm ${DB_HOME}-optimal/*
		done
		#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./output/
		#cd output-bpk/
		#./grep_accessed_data_blocks_by_bpk_microbenchmark.sh ${ZD} ${Z} > data_blocks_ZD${ZD}_Z${Z}_result.txt
		#./grep_accessed_data_blocks_by_bpk_microbenchmark_NMP.sh ${ZD} ${Z} > data_blocks_ZD${ZD}_Z${Z}_NMP${NMP}_result.txt
		#./grep_query_latency_by_bpk_microbenchmark.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt
		#cd -
	done
	for bpk in ${bpk_list[@]}
	do
		cd output-bpk/
		./grep_accessed_data_blocks_by_Z_microbenchmark.sh ${ZD} ${bpk} > data_blocks_ZD${ZD}_bpk-${bpk}_result.txt
		./grep_query_latency_by_Z_microbenchmark.sh ${ZD} ${bpk} > query_latency_ZD${ZD}_bpk-${bpk}_result.txt
		cd -

	done
done
#cp ../workload_generator_scripts/ingestion_workload.txt output/
