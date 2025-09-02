#!/bin/bash

P=2048
B=128
T=4
E=128
bpk=20
ZD_list=("1" "0")
Z="0.0"
runs=3
N_list=("10000000" "20000000" "40000000" "80000000" "160000000" "320000000")
scale_label=("scale1x" "scale2x" "scale4x" "scale8x" "scale16x" "scale32x")
#N_list=("40000000")
#scale_label=("scale4x")
output_dir="output-stats-for-optimization-exp/"

# Remember to specify a path for your dedicated storage device
DB_HOME="${RAM_DB_HOME:-./db_working_home}"
echo "Building DB with path DB_HOME : ${DB_HOME}"
mkdir -p "${output_dir}"
num_scale=${#N_list[@]}
for (( j=0; j<num_scale; j++ )); do
	for i in `seq 1 ${runs}`; do
		cd ../workload_generator_scripts/
		./varying_distribution_query_workload_with_large_inserts.sh ${N_list[$j]}
		cd -	
		LOCAL_OUTPUT_DIR="${output_dir}/${scale_label[$j]}/test${i}/"
		mkdir -p ${LOCAL_OUTPUT_DIR}
		for ZD in ${ZD_list[@]}
		do
			echo "./bpk_benchmark -p ${DB_HOME} -E${E} -P${P} -T${T} -B ${B} --no_dynamic_cmpct --BCC 524288 -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_for_opt_bpk_solver_workload.txt --qwp ../workload_generator_scripts/ZD${ZD}_Z${Z}_for_opt_bpk_solver_query_workload.txt --dqs --olcs"
			./bpk_benchmark -p ${DB_HOME} -E${E} -P${P} -T${T} -B ${B} --no_dynamic_cmpct -V 1 --BCC 524288 -b ${bpk} --dd --iwp ../workload_generator_scripts/ingestion_for_opt_bpk_solver_workload.txt --qwp ../workload_generator_scripts/ZD${ZD}_Z${Z}_for_opt_bpk_solver_query_workload.txt --dqs --olcs --query-stats-path ${LOCAL_OUTPUT_DIR}/Z${Z}_ZD${ZD}_query_stats.txt --run-stats-op ${LOCAL_OUTPUT_DIR}/Z${Z}_ZD${ZD}_output.txt
		done
	done	
done
