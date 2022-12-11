#!/bin/bash

# Author: Hubertus Franke  (frankeh@nyu.edu)
# OUTDIR=${1:-.}
# shift
# SCHED=${*:-../src/iosched}

OUTDIR=$1
SCHED=$2

echo "outdir=<$OUTDIR> sched=<$SCHED>"

INS="`seq 0 9`"
INPRE="input"
OUTPRE="out"

# SCHEDS="i j s c f"
SCHEDS="i j"


############################################################################
#  NO TRACING 
############################################################################

# run with RFILE1 
#ulimit -v 300000   # just limit the processes 

for f in ${INS}; do
	for s in ${SCHEDS}; do 
		echo "${SCHED} -s${s} ${INPRE}${f}"
		${SCHED} -s${s} ${INPRE}${f} > ${OUTDIR}/${OUTPRE}_${f}_${s} 
	done
done

