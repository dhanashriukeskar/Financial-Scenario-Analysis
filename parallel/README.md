# Parallel Implementation (OpenMP)

`parallel.cpp` uses the same `evaluate_scenario()` as the sequential version and
replaces only the loop with `#pragma omp parallel for schedule(runtime)`.
The number of threads and the schedule (static, dynamic or guided) are command-line
arguments, so no recompiling is needed to test 1, 2, 4 or 8 threads.

## Build and run (from the repository root)

    g++ -O2 -std=c++17 -Wall -Wextra -fopenmp parallel/parallel.cpp -o parallel
    ./parallel <N> <M> <threads> <static|dynamic|guided> [seed] [--out file.csv]

Example: `./parallel 10000 200 8 static`

OpenMP was chosen because scenarios are independent and the target machine is a
single shared-memory CPU (see the main README).
