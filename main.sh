#!/bin/bash

runs=$1

for i in `seq 1 ${runs}`
do
	#cd /scratchHDDa/zczhu/K-V-Workload-Generator/
	#./bpk_workload.sh
	#cd -
	./exp.sh
	mkdir -p "output${i}"
	mv output/*.txt output${i}/
done
#python3 merge.py ${runs}
