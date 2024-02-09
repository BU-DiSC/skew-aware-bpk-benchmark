#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	cd /scratchHDDb/zczhu/K-V-Workload-Generator/
	./simple_workload.sh
	cd -
	./exp_simple.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
done
mkdir -p "agg_output_by_short_bpk_with_rw_latency/"
python3 merge_by_short_bpk_with_rw_latency.py ${runs} agg_output_by_short_bpk_with_rw_latency/
