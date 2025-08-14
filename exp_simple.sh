#!/bin/bash

P=4096
B=32
T=4
E=512
R=1
#BCC=131072
BCHPR="0.9"
BCC=262144
# Remember to specify a path for your dedicated storage device
#DB_HOME="./db_working_home"
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"

Z_list=("0.0" "0.5" "1.0")
#ZD_list=("0" "3")
ZD_list=("0" "1")
bpk_list=("2.0" "4.0" "6.0")
#bpk_list=("2.0" "3.0" "4.0" "5.0")
dir="output/"
mkdir -p "${dir}"
#cp output/*.sh ${dir}
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		for bpk in ${bpk_list[@]}
		do
			echo "./simple_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_simple_mixed_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --no_dynamic_cmpct --BCHPR ${BCHPR} --dw --dr > ${dir}/Z${Z}_ZD${ZD}_bpk-${bpk}_output_simple_mixed_workload.txt"
			./simple_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_simple_mixed_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --no_dynamic_cmpct --BCHPR ${BCHPR} --dw --dr > ${dir}/Z${Z}_ZD${ZD}_bpk-${bpk}_output_simple_mixed_workload.txt
			#./simple_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_simple_mixed_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --no_dynamic_cmpct > ${dir}/Z${Z}_ZD${ZD}_bpk-${bpk}_output_simple_mixed_workload.txt
			rm ${DB_HOME}/*
			rm ${DB_HOME}-monkey/*
			rm ${DB_HOME}-mnemosyne/*
			rm ${DB_HOME}-mnemosyne-plus/*
		done
		#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./${dir}/

                cd ${dir}/
	        ./grep_accessed_data_blocks_by_bpk.sh ${ZD} ${Z} > accessed_data_blocks_ZD${ZD}_Z${Z}_result.txt
	        ./grep_read_bytes_by_bpk.sh ${ZD} ${Z} > read_bytes_ZD${ZD}_Z${Z}_result.txt
	        ./grep_query_latency_by_bpk.sh ${ZD} ${Z} > query_latency_ZD${ZD}_Z${Z}_result.txt
	        ./grep_avg_latency_by_bpk.sh ${ZD} ${Z} > avg_latency_ZD${ZD}_Z${Z}_result.txt
	        ./grep_write_latency_by_bpk.sh ${ZD} ${Z} > write_latency_ZD${ZD}_Z${Z}_result.txt
	        cd -
	done
	
done
#cp ../workload_generator_scripts/ingestion_workload.txt ${dir}/
