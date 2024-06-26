#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	cd ../workload_generator_scripts/
	../workload_generator_scripts/plain_workload.sh
	cd -
	./exp_plain.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
done
mkdir -p "agg_output/"
python3 merge.py ${runs} agg_output/
