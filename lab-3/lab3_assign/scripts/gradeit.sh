#!/bin/bash

#example ./gradeit.sh dir1 dir2 logfile

DIR1=$1
DIR2=$2
LOG=${3:-${DIR2}/LOG.txt}

USEDIFF=0
DARGS=         # nothing
DARGS="-q --speed-large-files"         # the big files are killing us --> out of memory / fork refused etc

INPUTS=`seq 1 10`
ALGOS=" f"
FRAMES="16 32"

declare -ai counters
declare -i x=0
for s in ${ALGOS}; do
        let counters[$x]=0
        let x=$x+1
done

echo "gradeit.sh ${DIR1} ${DIR2} ${LOG}" > ${LOG}

echo "input  frames   ${ALGOS}"

rm -fR ${DIR2}/LOG   # remove old LOGFILE

for I in ${INPUTS}; do
  for N in ${FRAMES}; do
    OUTLINE=`printf "%-7s %-7s" "${I}" "${N}"`
    x=0
    for A in ${ALGOS}; do 
	OUTF="out${I}_${N}_${A}"
        if [[ ! -e ${DIR1}/${OUTF} ]]; then
            echo "${DIR1}/${OUTF} does not exist" >> ${LOG}
            OUTLINE=`printf "%s o" "${OUTLINE}"`
            continue;
        fi;
        if [[ ! -e ${DIR2}/${OUTF} ]]; then
            echo "${DIR2}/${OUTF} does not exist" >> ${LOG}
            OUTLINE=`printf "%s o" "${OUTLINE}"`
            continue;
        fi;
	
#       echo "diff -b ${DARGS} ${DIR1}/${OUTF} ${DIR2}/${OUTF}"
#       diff hangs .. cmp does the same trick as we only what see whether it diffes
        if [[ ${USEDIFF} -eq 1 ]]; then
            DIFFCMD="diff -b ${DARGS} ${DIR1}/${OUTF} ${DIR2}/${OUTF}"
            DIFF=$(${DIFFCMD})
        else
            DIFFCMD="cmp ${DIR1}/${OUTF} ${DIR2}/${OUTF} 2>&1"
            DIFF=$(cmp ${DIR1}/${OUTF} ${DIR2}/${OUTF} 2>&1)
        fi
        if [[ $? == 0 ]]; then
            OUTLINE=`printf "%s  ." "${OUTLINE}"`
            let counters[$x]=`expr ${counters[$x]} + 1`
        else
            echo "diff -b ${DARGS} ${DIR1}/${OUTF} ${DIR2}/${OUTF} failed" >> ${LOG}
            echo "${DIFFCMD} failed" >> ${LOG}
            SUMX=`egrep "^TOTAL" ${DIR1}/${OUTF}`
            SUMY=`egrep "^TOTAL" ${DIR2}/${OUTF}`
            echo "   DIR1-SUM ==> ${SUMX}" >> ${LOG}
            echo "   DIR2-SUM ==> ${SUMY}" >> ${LOG}
            OUTLINE=`printf "%s  #" "${OUTLINE}"`
        fi
	let x=$x+1
    done
    echo "${OUTLINE}"
  done
done


OUTLINE=`printf "%-15s" "SUM"`
x=0
for A in ${ALGOS}; do 
    OUTLINE=`printf "%s %2d" "${OUTLINE}" "${counters[$x]}"`
    let x=$x+1
done
echo "${OUTLINE}"

