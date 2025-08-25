#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	echo "[Run ${i}] for comparing unnecessary I/Os with different bpk allocation policy"
	cd ../workload_generator_scripts/
	../workload_generator_scripts/bpk_workload.sh
	cd -
	./exp_Z.sh
	mkdir -p "output-vary-Z${i}"
	mv output-vary-Z/*.txt output-vary-Z${i}/
done
mkdir -p "agg_output_by_Z/"
python3 merge_by_Z_microbenchmark.py ${runs} agg_output_by_Z/ output-vary-Z
echo "Aggregated results have been written to agg_output_by_Z/"
