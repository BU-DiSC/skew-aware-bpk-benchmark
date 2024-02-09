#!/bin/bash

P=4096
B=8
T=4
E=1024
R=1
bpk=6
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"

Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "3")
#Z_list=("0.5")
#ZD_list=("0" "3")

mkdir -p "output/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		echo "./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp /scratchHDDb/zczhu/K-V-Workload-Generator/ingestion_workload.txt --qwp /scratchHDDb/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P 4096 -b ${bpk} --BCC 131072 -V 1 --no_dynamic_cmpct --dw --dr > output/Z${Z}_ZD${ZD}_output.txt"
		./plain_benchmark -T ${T} -E ${E} --dd -p ${DB_HOME} --iwp /scratchHDDb/zczhu/K-V-Workload-Generator/ingestion_workload.txt --qwp /scratchHDDb/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt -B ${B} -P 4096 -b ${bpk} --BCC 131072 -R ${R} -V 1 --no_dynamic_cmpct --dw --dr > output/Z${Z}_ZD${ZD}_output.txt
		rm ${DB_HOME}/*
		rm ${DB_HOME}-monkey/*
		rm ${DB_HOME}-workloadaware/*
		#cp /scratchHDDb/zczhu/K-V-Workload-Generator/Z${Z}_ZD${ZD}_query_workload.txt ./output/
	done
	#cd output/
	#./grep_accessed_data_blocks.sh ${ZD} > data_blocks_ZD${ZD}_result.txt
	#cd -
done
#cp /scratchHDDb/zczhu/K-V-Workload-Generator/ingestion_workload.txt output/
