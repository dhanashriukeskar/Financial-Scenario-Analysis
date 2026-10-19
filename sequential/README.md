# Sequential Implementation

`sequential.cpp` evaluates N financial scenarios one after another on a single
thread using `evaluate_scenario()` from `common/scenario.h`. It is the baseline
(Ts) for speedup and efficiency, and the reference for checking the parallel version.

`test_sequential.cpp` has 17 unit tests (edge cases, determinism, classification).

## Build and run (from the repository root)

    g++ -O2 -std=c++17 -Wall -Wextra sequential/sequential.cpp -o sequential
    g++ -O2 -std=c++17 -Wall -Wextra sequential/test_sequential.cpp -o test_sequential
    ./sequential <N> <M> [seed] [--out file.csv]
    ./test_sequential
