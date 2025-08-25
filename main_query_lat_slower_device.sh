#!/bin/bash

runs=$1
for i in `seq 1 ${runs}`
do
	./exp_query_lat_slower_device.sh
	mkdir -p "output-bpk-slower-device${i}"
	mv output-bpk-slower-device/*.txt output-bpk-slower-device${i}/
done
mkdir -p "agg_output_slower_device/"
python3 merge_by_short_bpk_with_rw_latency.py ${runs} agg_output_slower_device/ output-bpk-slower-device --Z-list 1.0
