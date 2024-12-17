#!/bin/bash

# Fabiano A. de Sá Filho
# GRR: 20223831

make clean
make

parts=("parteA.txt" "parteB.txt")
sizes=(1000 100000)

main_script="./roda-todos-slurmMOD.sh" 

if [ ! -x "$main_script" ]; then
    echo "Error: $main_script not found or not executable."
    exit 1
fi

for part in "${parts[@]}"; do
    for size in "${sizes[@]}"; do
        echo "Running tests for $part with size $size..."
        $main_script "$size" "$part"
        echo "Completed tests for $part with size $size."
    done
done