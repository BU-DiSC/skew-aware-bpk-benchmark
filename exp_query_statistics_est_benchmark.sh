#!/bin/bash

P=1024
B=32
T=4
E=512
epri=200000
R=1
# Remember to specify a path for your RAM disk
DB_HOME="/scratchNVM0/zczhu/test_db_dir/db_working_home"
DB_HOME="/mnt/ramd/zczhu/db_working_home"
Z_list=("0.0" "0.5" "1.0")
ZD_list=("0" "1")
#Z_list=("0.0")
#ZD_list=("3")
mkdir -p "query_statistics_est_benchmark_result/"
for ZD in ${ZD_list[@]}
do
	for Z in ${Z_list[@]}
	do
		echo "./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --epri ${epri} -R ${R} -V 1"
		./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --epri ${epri} -R ${R} -V 1
		mv num_point_reads_stats_diff.txt query_statistics_est_benchmark_result/top_down_Z${Z}_ZD${ZD}_num_point_reads_stats_diff.txt
		mv num_empty_point_reads_stats_diff.txt query_statistics_est_benchmark_result/top_down_Z${Z}_ZD${ZD}_num_empty_point_reads_stats_diff.txt
		rm ${DB_HOME}/*
		rm ${DB_HOME}-temp/*
		rm ${DB_HOME}-to-be-eval/*	


		echo "./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --epri ${epri} -R ${R} -V 1"
		./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload.txt --qwp ../workload_generator_scripts/Z${Z}_ZD${ZD}_query_workload.txt --epri ${epri} -R ${R} -V 1
		mv num_point_reads_stats_diff.txt query_statistics_est_benchmark_result/bottom_up_Z${Z}_ZD${ZD}_num_point_reads_stats_diff.txt
		mv num_empty_point_reads_stats_diff.txt query_statistics_est_benchmark_result/bottom_up_Z${Z}_ZD${ZD}_num_empty_point_reads_stats_diff.txt
		rm ${DB_HOME}/*
		rm ${DB_HOME}-temp/*
		rm ${DB_HOME}-to-be-eval/*	
	done
done
