#!/bin/bash

P=1024
B=64
T=4
E=128
R=3
BCC=131072
#BCC=262144
# Remember to specify a path for your dedicated storage device
DB_HOME="/scratchNVM1/zczhu/test_db_dir/db_working_home"
#DB_HOME="/mnt/ramd/zczhu/db_working_home"
throughput_interval=100000
bpk_list=("2.0" "4.0" "6.0")
#bpk_list=("20.0")
dir="exp-throughputs-and-bpk-for-mixed-empty-workloads/"
WORKLOAD="../workload_generator_scripts/workload_type_I.txt"
#WORKLOAD="../workload_generator_scripts/small_workload_type_I.txt"
WORKLOAD_NAME="workload_type_I"
mkdir -p "${dir}"
#cp output/*.sh ${dir}
for bpk in ${bpk_list[@]}
do
	echo "./runtime_tput_exp -T ${T} -E ${E} --dd --no_dynamic_cmpct -p ${DB_HOME} --qwp ${WORKLOAD} -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -V 1 --dw --dr --clct-tputi ${throughput_interval} --stats-op ${dir}/${WORKLOAD_NAME}_output_bpk-${bpk}.txt"
	./runtime_tput_exp -T ${T} -E ${E} --dd --no_dynamic_cmpct -p ${DB_HOME} --qwp ${WORKLOAD} -B ${B} -P ${P} -b ${bpk} --BCC ${BCC} -R ${R} -V 1 --dw --dr --clct-tputi ${throughput_interval} --stats-op ${dir}/${WORKLOAD_NAME}_output_bpk-${bpk}.txt
	mv throughputs.txt ${dir}/${WORKLOAD_NAME}-bpk-${bpk}_throughputs.txt
	mv tracked_avg_bpk.txt ${dir}/${WORKLOAD_NAME}-bpk-${bpk}_tracked_avg_bpk.txt
	
	rm ${DB_HOME}/*
	rm ${DB_HOME}-monkey-top-down/*
	rm ${DB_HOME}-monkey-bottom-up/*
	rm ${DB_HOME}-mnemosyne/*
done	
#cp ../workload_generator_scripts/ingestion_workload.txt ${dir}/
