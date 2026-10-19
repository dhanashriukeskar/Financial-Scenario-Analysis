#!/usr/bin/env bash
# Build, test, then run sequential vs parallel with 1, 2, 4, 8 threads.
set -e
bash build.sh
N=${1:-10000}; M=${2:-200}
echo; echo "=== Unit tests ==="; ./bin/test_sequential | tail -1
echo; echo "=== Sequential  N=$N M=$M ==="; ./bin/sequential $N $M
for T in 1 2 4 8; do
  echo; echo "=== Parallel  N=$N M=$M threads=$T static ==="
  ./bin/parallel $N $M $T static
done
echo; echo "=== Correctness check (sequential vs OpenMP) ==="; ./bin/verify 1000 200
echo; echo "Thread study with median of repeats (writes results/my_benchmark_threads.csv):"
echo "  ./bin/benchmark threads 200 7 results/my_benchmark_threads.csv"
