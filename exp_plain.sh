#!/bin/bash

P=4096
B=8
T=4
E=1024
R=1
bpk=6
BCC="131072"
# Remember to specify a path for your dedicated storage device
DB_HOME="./db_working_home"

Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "3")
Z_list=("0.5")
ZD_list=("0")

mkdir -p "output/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		echo "./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P 4096 -b ${bpk} --BCC ${BCC} -V 1 --no_dynamic_cmpct --dw --dr > output/Z${Z}_ZD${ZD}_output.txt"
		#./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P 4096 -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --no_dynamic_cmpct --dw --dr > output/Z${Z}_ZD${ZD}_output.txt
		rm ${DB_HOME}/*
		rm ${DB_HOME}-monkey/*
		rm ${DB_HOME}-workloadaware/*
		#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./output/
	done
	#cd output/
	#./grep_accessed_data_blocks.sh ${ZD} > data_blocks_ZD${ZD}_result.txt
	#cd -
done
#cp ../workload_generator_scripts/ingestion_workload.txt output/
