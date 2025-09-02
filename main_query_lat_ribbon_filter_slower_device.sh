#!/bin/bash

runs=$1
for i in `seq 1 ${runs}`
do
	./exp_query_lat_ribbon_filter.sh
	mkdir -p "output-bpk-ribbon-filter${i}"
	mv output-bpk-ribbon-filter/*.txt output-bpk-ribbon-filter${i}/
done
mkdir -p "agg_output_ribbon_filter/"
python3 merge_by_short_bpk_with_rw_latency.py ${runs} agg_output_ribbon_filter/ output-bpk-ribbon-filter --Z-list 1.0 --ZD-list 0
