#!/bin/bash

P=4096
B=16
T=4
E=512
epri=200000
R=3
# Remember to specify a path for your RAM disk
DB_HOME="./db_working_home"
#Z_list=("0.0")
#ZD_list=("3")
cd ../workload_generator_scripts
N=4000000
Q=6000000
U=10000000
sed -i '2s/I=".*"/I="XXX\"/' context_switch_workload_distribution.sh
sed -i "2s/XXX/${N}/" context_switch_workload_distribution.sh
sed -i '3s/Q=".*"/Q="YYY\"/' context_switch_workload_distribution.sh
sed -i "3s/YYY/${Q}/" context_switch_workload_distribution.sh
sed -i '4s/U=".*"/U="YYY\"/' context_switch_workload_distribution.sh
sed -i "4s/YYY/${U}/" context_switch_workload_distribution.sh
./context_switch_workload_distribution.sh
cd -
echo "./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload_for_shifting_distribution.txt --qwp ../workload_generator_scripts/context_switch_queries_distribution.txt --epri ${epri} -R ${R} -V 1"
./query_statistics_est_benchmark -E${E} -P${P} -T${T} -B ${B} -p ${DB_HOME} --no_dynamic_cmpct --BCC 2097152 -b 20 --iwp ../workload_generator_scripts/ingestion_workload_for_shifting_distribution.txt --qwp ../workload_generator_scripts/context_switch_queries_distribution.txt --epri ${epri} -R ${R} -V 1 | tee query_statistics_est_benchmark_result/shifting_distribution_output.txt
mv num_point_reads_stats_diff.txt query_statistics_est_benchmark_result/shifting_distribution_num_point_reads_stats_diff.txt
mv num_empty_point_reads_stats_diff.txt query_statistics_est_benchmark_result/shifting_distribution_num_empty_point_reads_stats_diff.txt
	
rm ${DB_HOME}/*
rm ${DB_HOME}-temp/*
rm ${DB_HOME}-to-be-eval/*	
