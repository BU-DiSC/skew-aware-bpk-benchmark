#!/bin/bash

P=1024
B=32
T=4
E=512
R=1
bpk=6
bpk_list=("2" "3" "4" "5" "6" "7")
#bpk_list=("20" "21")
#bpk_list=("10")
BCC="524288"
# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="/mnt/ramd/zczhu/db_working_home2"
WORKLOAD_HOME="/scratchHDDa/zczhu/dynamic_bpk_emulation/workload_generator_scripts"
Z_list=("1.0" "0.5" "0.0")
ZD_list=("0" "1")
#Z_list=("0.0")
#ZD_list=("3")
NDEV="3.0"
OUTPUT_DIR="plain-output-bpk"
mkdir -p "${OUTPUT_DIR}"
#ulimit -n 65535
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			if [ "${ZD}" == "1" ]; then
				echo "./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dr > ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_NDEV${NDEV}_bpk-${bpk}-output.txt"
				./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dr > ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_NDEV${NDEV}_bpk-${bpk}-output.txt
				#echo "./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dw --dr > ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_NDEV${NDEV}_bpk-${bpk}-output.txt"
				#./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_NDEV${NDEV}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dw --dr > ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_NDEV${NDEV}_bpk-${bpk}-output.txt
			else
				echo "./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dr > ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_bpk-${bpk}-output.txt"
				./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ${WORKLOAD_HOME}/ingestion_workload.txt --qwp ${WORKLOAD_HOME}/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dr > ${OUTPUT_DIR}/Z${Z}_ZD${ZD}_bpk-${bpk}-output.txt
			fi
			rm ${DB_HOME}/*
			rm ${DB_HOME}-mnemosyne/*
			rm ${DB_HOME}-mnemosyne-plus/*
		done
		cp output/*.sh ${OUTPUT_DIR}/
		cd ${OUTPUT_DIR}/
		echo "./grep_accessed_data_blocks_by_bpk.sh ${ZD} ${Z} > accessed_data_blocks_ZD${ZD}_Z${Z}_result.txt"
		echo "./grep_read_bytes_by_bpk.sh ${ZD} ${Z} > read_bytes_ZD${ZD}_Z${Z}_result.txt"
		echo "./grep_query_latency_by_bpk.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt"
		echo "./grep_avg_latency_by_bpk.sh ${ZD} ${Z} > avg_latency_ZD${ZD}_Z${Z}_result.txt"
		echo "./grep_write_latency_by_bpk.sh ${ZD} ${Z} > write_latency_ZD${ZD}_Z${Z}_result.txt"
		./grep_accessed_data_blocks_by_bpk.sh ${ZD} ${Z} > accessed_data_blocks_ZD${ZD}_Z${Z}_result.txt
		./grep_read_bytes_by_bpk.sh ${ZD} ${Z} > read_bytes_ZD${ZD}_Z${Z}_result.txt
		./grep_query_latency_by_bpk.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt
		./grep_avg_latency_by_bpk.sh ${ZD} ${Z} > avg_latency_ZD${ZD}_Z${Z}_result.txt
		./grep_write_latency_by_bpk.sh ${ZD} ${Z} > write_latency_ZD${ZD}_Z${Z}_result.txt
		cd -
	done
done
#cp ../workload_generator_scripts/ingestion_workload.txt output/
