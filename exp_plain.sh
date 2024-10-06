#!/bin/bash

P=4096
B=16
T=4
E=512
R=1
bpk=6
bpk_list=("2" "3" "4" "5" "6" "7")
BCC="262144"
# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="/mnt/ramd/zczhu/db_working_home"

Z_list=("1.0" "0.5" "0.0")
ZD_list=("0" "3")
#Z_list=("0.5")
#ZD_list=("0")

mkdir -p "output/"
for bpk in ${bpk_list[@]}
do
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		echo "./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dw --dr > output-test-skipping/Z${Z}_ZD${ZD}_bpk-${bpk}-output.txt"
		./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 > output-test-skipping/Z${Z}_ZD${ZD}_bpk-${bpk}-output.txt
		rm ${DB_HOME}/*
		rm ${DB_HOME}-monkey/*
		rm ${DB_HOME}-monkey-plus/*
		rm ${DB_HOME}-workloadaware/*
		#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./output/
	done
	#cd output/
	#./grep_accessed_data_blocks.sh ${ZD} > data_blocks_ZD${ZD}_result.txt
	#cd -
done
done
#cp ../workload_generator_scripts/ingestion_workload.txt output/
