#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	./exp_bpk.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
	cd ../workload_generator_scripts/
	../workload_generator_scripts/bpk_workload.sh
	cd -
done
mkdir -p "agg_output_by_bpk/"
python3 merge_by_bpk.py ${runs} agg_output_by_bpk/
