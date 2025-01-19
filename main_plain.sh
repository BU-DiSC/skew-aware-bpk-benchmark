#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	./exp_plain.sh
	mkdir -p "output${i}"
	mv plain-output-bpk/*.txt plain-output-bpk${i}/
	cd ../workload_generator_scripts/
	../workload_generator_scripts/plain_workload.sh
	cd -
done
mkdir -p "agg_output/"
python3 merge_by_short_bpk_with_rw_latency.py ${runs} agg_output/ plain-output-bpk
