# How to run the code

Requirements: g++ with OpenMP (g++ 13 used in the report), C++17.

## Quick start
Linux / macOS / MSYS2 shell:   `bash run_demo.sh`            (default N=10000, M=200)
                               `bash run_demo.sh 100000 200`  (custom N and M)
Windows PowerShell:            `.\run_demo.bat`   (the .\ is required in PowerShell)
Windows CMD:                   `run_demo.bat`

This builds everything into `bin/`, runs the 17 unit tests, runs the sequential
program, runs the parallel program with 1, 2, 4 and 8 threads, and runs the
sequential-vs-OpenMP correctness check.

## Programs (after build)
    bin/sequential <N> <M> [seed] [--out file.csv]
    bin/parallel   <N> <M> <threads> <static|dynamic|guided> [seed] [--out file.csv]
    bin/verify     <N> <M> [seed]
    bin/benchmark  <scale|threads|schedule|all> [M] [repeats] [out.csv]
    bin/test_sequential

N = number of scenarios, M = Monte Carlo paths per scenario, default seed 12345.
On Windows use `bin\sequential.exe`, etc.

## Experiments in the report
    bin/benchmark scale    200 7 results/my_benchmark_scale.csv      (Experiment 1: time vs N, 8 threads)
    bin/benchmark threads  200 7 results/my_benchmark_threads.csv    (Experiment 2: 1, 2, 4, 8 threads, N=10,000)
    bin/benchmark schedule 200 9 results/my_benchmark_schedule.csv   (Experiment 3: static/dynamic/guided)

Give an output file name as shown; otherwise the benchmark overwrites the
original CSVs in results/ that the report's graphs came from.
Thread counts above omp_get_max_threads() are skipped, so a machine with
fewer than 8 logical processors will test fewer thread counts.
For stable timings: close other applications and plug in the laptop.

## Contents
    common/      shared model and evaluate_scenario() (scenario.h)
    sequential/  sequential.cpp, test_sequential.cpp
    parallel/    parallel.cpp (OpenMP)
    verify/      verify.cpp
    benchmark/   benchmark.cpp
    results/     original CSVs and graphs used in the report
    docs/        project_notes.md
Note: parallel/README.md is an older placeholder; parallel.cpp itself is complete.
