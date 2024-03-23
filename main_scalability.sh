#!/bin/bash

runs=$1


for i in `seq 1 ${runs}`
do
	./scalability.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
done
mkdir -p "agg_scalability/"
python3 merge_scalability.py ${runs} agg_scalability/
