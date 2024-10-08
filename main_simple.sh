#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	cd ../workload_generator_scripts/
	../workload_generator_scripts/simple_workload.sh
	cd -
	./exp_simple.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
done
mkdir -p "agg_output_by_short_bpk_with_rw_latency_skip_filter_when_insufficient_reads_128MB_bc/"
python3 merge_by_short_bpk_with_rw_latency.py ${runs} agg_output_by_short_bpk_with_rw_latency_skip_filter_when_insufficient_reads_128MB_bc/
