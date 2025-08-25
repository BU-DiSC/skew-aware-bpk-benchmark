#!/bin/bash

P=1024
B=32
T=4
E=512
bpk=20
NDEV_list=("3.0" "4.0" "5.0")
ZD_list=("1" "0")
output_dir="output-stats/"

# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="/mnt/ramd/zczhu/db_working_home"
mkdir -p "${output_dir}"
for ZD in ${ZD_list[@]}
do
	if [ ${ZD} == "1" ]; then
		for NDEV in ${NDEV_list[@]}
		do
			echo "./bpk_benchmark -p ${DB_HOME} -E${E} -P${P} -T${T} -B ${B} --no_dynamic_cmpct --BCC 524288 -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Normal_Dis_NDEV${NDEV}_empty_query_workload.txt --dqs --olcs"
			./bpk_benchmark -p ${DB_HOME} -E${E} -P${P} -T${T} -B ${B} --no_dynamic_cmpct -V 1 --BCC 524288 -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Normal_Dis_NDEV${NDEV}_empty_query_workload.txt --dqs --olcs --query-stats-path ${output_dir}/Normal_Dis_NDEV${NDEV}_query_stats.txt --stats-op ${output_dir}/Normal_Dis_NDEV${NDEV}_output.txt
		done
	else
		echo "./bpk_benchmark -p ${DB_HOME} -E${E} -P${P} -T${T} -B ${B} --no_dynamic_cmpct --BCC 524288 -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Uniform_Dis_empty_query_workload.txt --dqs --olcs"
		./bpk_benchmark -p ${DB_HOME} -E${E} -P${P} -T${T} -B ${B} --no_dynamic_cmpct -V 1 --BCC 524288 -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Uniform_Dis_empty_query_workload.txt --dqs --olcs --query-stats-path ${output_dir}/Uniform_Dis_query_stats.txt --stats-op ${output_dir}/Uniform_Dis_output.txt
	fi
	rm ${DB_HOME}/*
	#cp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt ./output/
done
#cp ../workload_generator_scripts/ingestion_workload.txt output/
