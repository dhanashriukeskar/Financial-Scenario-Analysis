#!/usr/bin/env bash
# Compile all programs (run from this folder). Needs g++ with OpenMP.
set -e
mkdir -p bin
F="-O2 -std=c++17 -Wall -Wextra"
g++ $F           sequential/sequential.cpp      -o bin/sequential
g++ $F           sequential/test_sequential.cpp -o bin/test_sequential
g++ $F -fopenmp  parallel/parallel.cpp          -o bin/parallel
g++ $F -fopenmp  verify/verify.cpp              -o bin/verify
g++ $F -fopenmp  benchmark/benchmark.cpp        -o bin/benchmark
echo "Build OK -> bin/"
