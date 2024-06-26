#!/bin/bash

P=4096
B=16
T=4
E=1024
epri=200000
R=3
# Remember to specify a path for your RAM disk
DB_HOME="./db_working_home"
Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "3")
#Z_list=("0.0")
#ZD_list=("3")
mkdir -p "output/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		echo "./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --epri ${epri} -R ${R} -V 1"
		./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --epri ${epri} -R ${R} -V 1
		mv num_point_reads_stats_diff.txt query_statistics_est_benchmark_result/Z${Z}_ZD${ZD}_num_point_reads_stats_diff.txt
		mv num_empty_point_reads_stats_diff.txt query_statistics_est_benchmark_result/Z${Z}_ZD${ZD}_num_empty_point_reads_stats_diff.txt
		#rm ${DB_HOME}/*
		#rm ${DB_HOME}-temp/*
		#rm ${DB_HOME}-to-be-eval/*	
	done
done
