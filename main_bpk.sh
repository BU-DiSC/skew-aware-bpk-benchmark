#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	cd /scratchHDDa/zczhu/K-V-Workload-Generator/
	./bpk_workload.sh
	cd -
	./exp_bpk.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
done
mkdir -p "agg_output_by_bpk/"
python3 merge_by_bpk.py ${runs} agg_output_by_bpk/
