#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	./exp_query_lat.sh
	mkdir -p "output-bpk${i}"
	mv output-bpk/*.txt output-bpk${i}/
	cd ../workload_generator_scripts/
	../workload_generator_scripts/workload_type_II.sh
	cd -
done
mkdir -p "agg_output/"
python3 merge_by_short_bpk_with_rw_latency.py ${runs} agg_output/ output-bpk
