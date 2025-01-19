#!/bin/bash

P=1024
B=32
T=4
E=128
R=3
BCC=131072
#BCC=262144
# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="/mnt/ramd/zczhu/db_working_home"
throughput_interval=100000
ZD_list=("0" "1")
#ZD_list=("1")
#ZD_list=("1")
#bpk_list=("2.0" "3.0" "4.0" "5.0" "6.0" "7.0")
bpk_list=("2.0" "4.0" "6.0")
#bpk_list=("7.0")
dir="exp-throughputs-and-bpk-for-mixed-empty-workloads/"
NDEV="5.0"
mkdir -p "${dir}"
#cp output/*.sh ${dir}
for ZD in ${ZD_list[@]}
do
	for bpk in ${bpk_list[@]}
	do
		if [ ${ZD} == "1" ]; then
			echo "./simple_benchmark_2 -T ${T} -E ${E} --dd --no_dynamic_cmpct -p ${DB_HOME} --qwp ../workload_generator_scripts/ZD${ZD}_NDEV${NDEV}_E${E}_mixed_empty_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dw --dr --clct-tputi ${throughput_interval} > ${dir}/ZD${ZD}_NDEV${NDEV}_bpk-${bpk}_output_mixed_empty_query_workload.txt"
			./simple_benchmark_2 -T ${T} -E ${E} --dd --no_dynamic_cmpct -p ${DB_HOME} --qwp ../workload_generator_scripts/ZD${ZD}_NDEV${NDEV}_E${E}_mixed_empty_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dw --dr --clct-tputi ${throughput_interval} > ${dir}/ZD${ZD}_NDEV${NDEV}_bpk-${bpk}_output_mixed_empty_query_workload.txt
			mv throughputs.txt ${dir}/ZD${ZD}-NDEV${NDEV}-bpk-${bpk}_throughputs_mixed_empty_query_workload_exp.txt
			mv tracked_avg_bpk.txt ${dir}/ZD${ZD}-NDEV${NDEV}-bpk-${bpk}_tracked_avg_bpk_empty_query_workload_exp.txt
		else
			echo "./simple_benchmark_2 -T ${T} -E ${E} --dd --no_dynamic_cmpct -p ${DB_HOME} --qwp ../workload_generator_scripts/ZD${ZD}_E${E}_mixed_empty_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dw --dr --clct-tputi ${throughput_interval} > ${dir}/ZD${ZD}_bpk-${bpk}_output_mixed_empty_query_workload.txt"
			./simple_benchmark_2 -T ${T} -E ${E} --dd --no_dynamic_cmpct -p ${DB_HOME} --qwp ../workload_generator_scripts/ZD${ZD}_E${E}_mixed_empty_query_workload.txt -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dw --dr --clct-tputi ${throughput_interval} > ${dir}/ZD${ZD}_bpk-${bpk}_output_mixed_empty_query_workload.txt
			mv throughputs.txt ${dir}/ZD${ZD}-bpk-${bpk}_throughputs_mixed_empty_query_workload_exp.txt
			mv tracked_avg_bpk.txt ${dir}/ZD${ZD}-bpk-${bpk}_tracked_avg_bpk_empty_query_workload_exp.txt
		fi
		rm ${DB_HOME}/*
		rm ${DB_HOME}-monkey-top-down/*
		rm ${DB_HOME}-monkey-bottom-up/*
		rm ${DB_HOME}-mnemosyne/*
	
	done	
done
#cp ../workload_generator_scripts/ingestion_workload.txt ${dir}/
