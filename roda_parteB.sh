#!/bin/bash
echo "USAGE: ./rodaTodos.sh"
if [ "$#" -ne 0 ]; then
    echo "Error: Missing arguments."
    echo "Usage: ./rodaTodos.sh <nElements> <outputFile>"
    exit 1
fi

nElements=100000
outputFile=parteB.txt

echo "$0 rodando no host " `hostname`  
echo "$0 rodando no host " `hostname` >"$outputFile"

echo "SLURM_JOB_NAME: " $SLURM_JOB_NAME	
echo "SLURM_NODELIST: " $SLURM_NODELIST 
echo "SLURM_JOB_NODELIST: " $SLURM_JOB_NODELIST
echo "SLURM_JOB_CPUS_PER_NODE: " $SLURM_JOB_CPUS_PER_NODE

NTIMES=10
echo "nt " $NTIMES
MAX_EXECS=5
echo "MAX_EXECS " $MAX_EXECS

for i in {1..8}
do
    echo "Executando $NTIMES vezes com $nElements elementos e $i threads:"
    for j in $(seq 1 $NTIMES);
    do
        echo "-----------------------" >>"$outputFile"
        if [ $j -le $MAX_EXECS ]; then 
          ./multi_partition $nElements $i | tee -a "$outputFile" | grep -oP '(?<=total_time_in_seconds: )[^ ]*'
        else
          echo "nao executado" | tee -a "$outputFile" 
        fi  
    done
done

echo "O tempo total dessa shell foi de" $SECONDS "segundos"
echo "SLURM_JOB_NAME: "	$SLURM_JOB_NAME	
echo "SLURM_NODELIST: " $SLURM_NODELIST 
echo "SLURM_JOB_NODELIST: " $SLURM_JOB_NODELIST
echo "SLURM_JOB_CPUS_PER_NODE: " $SLURM_JOB_CPUS_PER_NODE

# imprime infos do job slurm (e.g. TEMPO até aqui no fim do job)
squeue -j $SLURM_JOBID



