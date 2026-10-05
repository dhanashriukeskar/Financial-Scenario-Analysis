# Financial Scenario Analysis: Sequential vs OpenMP

DAA / Parallel Programming Mini Project

## Problem Statement
Develop a computational system for evaluating a large number of financial
scenarios according to specified criteria. Investigate how execution time
changes with the number of scenarios.

## Objective
Evaluate many independent financial scenarios, then measure how execution
time changes with (1) the number of scenarios, (2) the number of OpenMP
threads, and (3) the OpenMP scheduling strategy. Report speedup and parallel
efficiency, and verify that the parallel results match the sequential ones.

## Technologies
C++17, OpenMP, g++ 13.2.0 (MSYS2), VS Code. No other libraries.

## Repository Structure

```text
Financial-Scenario-Analysis/
├── common/       shared scenario model and evaluation code (scenario.h)
├── sequential/   sequential.cpp, test_sequential.cpp
├── parallel/     parallel.cpp (OpenMP)
├── verify/       verify.cpp (sequential vs OpenMP correctness check)
├── benchmark/    benchmark.cpp (scale, threads, schedule experiments)
├── data/         input data notes (scenarios are generated from a fixed seed)
├── results/      final benchmark CSVs and graphs
├── report/       report and LLM usage log
├── docs/         project_notes.md
└── README.md
```

## Financial Model (see docs/project_notes.md)
Each scenario has: initial investment P, expected annual return r, risk
sigma (volatility), duration T years, and inflation rate.
For each scenario, M Monte Carlo paths are simulated. Yearly log-returns are
Normal(log(1+r) - sigma^2/2, sigma). Each path's final value is deflated by
inflation. From the M paths we compute: mean real final value, profit, ROI,
annual real return, probability of loss, and a score (ROI / stdev).

Classification criteria (constants at the top of common/scenario.h):
- RECOMMENDED: probability of loss <= 0.20 and annual real return >= 3%
- ACCEPTABLE: probability of loss <= 0.40 and annual real return >= 0
- REJECTED: otherwise

NOTE (assumption): this project uses the team's Monte Carlo design from
docs/project_notes.md, not an NPV / Future Value model.

## Choice of Parallel Programming Model
OpenMP was chosen because:
- Every scenario is independent, with no communication between scenarios
  (data parallelism).
- The target machine is a single shared-memory CPU (4 cores, 8 threads).
- One pragma parallelizes the loop, so sequential and parallel code stay
  almost identical and are easy to verify.

MPI targets distributed memory across processes or machines, which this
setup does not have. CUDA is not suitable for the available hardware (Intel
UHD Graphics has no CUDA support). Neither was tested.

## Sequential Approach
sequential/sequential.cpp loops over all scenarios in order and calls
evaluate_scenario() for each one.

## OpenMP Parallel Approach
parallel/parallel.cpp uses the same evaluate_scenario() and replaces only the
loop with `#pragma omp parallel for schedule(runtime)`.
Why the loop is safe: iteration i reads scenarios[i] and writes results[i]
only, and each scenario uses its own random generator seeded with
(seed + id). No data is shared, so there is no race condition and no
critical section.
verify/verify.cpp also demonstrates `reduction(+:...)` for class counts and
sums.

## Compilation
Run from the repository root.

    g++ -O2 -std=c++17 -Wall -Wextra sequential/sequential.cpp -o sequential
    g++ -O2 -std=c++17 -Wall -Wextra sequential/test_sequential.cpp -o test_sequential
    g++ -O2 -std=c++17 -Wall -Wextra -fopenmp parallel/parallel.cpp -o parallel
    g++ -O2 -std=c++17 -Wall -Wextra -fopenmp verify/verify.cpp -o verify
    g++ -O2 -std=c++17 -Wall -Wextra -fopenmp benchmark/benchmark.cpp -o benchmark

-O2 enables standard optimizations (kept identical for all builds).
-std=c++17 selects the language standard. -fopenmp enables OpenMP.
-Wall -Wextra show warnings.

## Execution

    ./sequential <N> <M> [seed] [--out file.csv]
    ./parallel <N> <M> <threads> <static|dynamic|guided> [seed] [--out file.csv]
    ./verify <N> <M> [seed]
    ./benchmark <scale|threads|schedule|all> [M] [repeats] [out.csv]
    ./test_sequential

N = number of scenarios, M = Monte Carlo paths per scenario (default seed 12345).
On Windows PowerShell use .\sequential.exe etc.

Example:

    .\sequential.exe 1000 1000
    .\parallel.exe 1000 1000 8 static
    .\verify.exe 1000 1000
    .\benchmark.exe threads 200 7

## Input / Output Format
Input: command-line numbers only; scenarios are generated from a fixed seed,
so no input file is required.
Output: summary (class counts, mean score, sum of mean finals, evaluation
time). With --out, a per-scenario CSV. benchmark writes
results/benchmark_<experiment>.csv with columns:
Experiment, Scenarios, Mode, Threads, Schedule, MedianTime, Speedup,
EfficiencyPct, Recommended, Acceptable, Rejected.

## Performance Metrics
- Speedup = Ts / Tp
- Efficiency (%) = Speedup / threads x 100

Only the evaluation loop is timed. Scenario generation, validation,
allocation, summaries and printing are outside the timer.

## Experimental Methodology
- Same dataset (seed 12345) for sequential and parallel runs.
- One warm-up run (discarded), then 7 timed runs (9 for the schedule test);
  the median is reported.
- Laptop plugged in, Windows power mode "Best performance", other apps
  closed. M = 200 paths per scenario.

System: Intel Core i5-10210U (4 cores, 8 logical processors, 6 MB L3),
8 GB DDR4-2667, Windows 11 Home 25H2 (build 26200.9457), g++ 13.2.0.

## Correctness
verify.exe compared sequential vs OpenMP for 1, 2, 4, 8 threads x 3
schedules (12 configurations) on N = 1000/M = 1000, N = 5000/M = 200 and
N = 1/M = 10. All class counts and every individual result matched exactly.
Reduction sums differed from in-order sums by at most 1.7e-15 (relative),
far below the 1e-9 tolerance. The tolerance is needed because floating-point
addition is not associative. test_sequential passes 17 tests, and per-scenario
CSVs from sequential and parallel runs were identical (fc.exe).

## Results (from results/*.csv)
Scale (8 threads, static, M = 200):

| N | Seq (s) | Par (s) | Speedup | Efficiency |
|---|---|---|---|---|
| 100 | 0.0156 | 0.0041 | 3.77 | 47% |
| 1,000 | 0.176 | 0.0297 | 5.93 | 74% |
| 5,000 | 0.671 | 0.132 | 5.08 | 64% |
| 10,000 | 1.081 | 0.265 | 4.07 | 51% |
| 50,000 | 5.430 | 1.270 | 4.28 | 53% |
| 100,000 | 10.699 | 2.632 | 4.07 | 51% |

Threads (N = 10,000): speedup 1.00, 1.85, 2.75, 4.13 for 1, 2, 4, 8 threads
(efficiency 100%, 92%, 69%, 52%).

Schedule (N = 10,000, 8 threads): static 0.227 s, dynamic 0.272 s,
guided 0.266 s.

## Observations
- Execution time grows roughly linearly with N (about 2x time for 2x
  scenarios between N = 50,000 and 100,000).
- At large N, speedup on 8 threads is about 4.1 to 4.3x.
- Efficiency falls as threads increase. This is consistent with 4 physical
  cores using hyper-threading; we did not measure this directly.
- Parallel was faster even at N = 100 (only N >= 100 was tested). Each
  scenario is expensive, so thread overhead is small compared with the work.
  A crossover where parallel loses may exist for much smaller N.
- Static was fastest in our runs, but the same configuration varied by up to
  about 20 to 35% between runs, so we cannot claim a real advantage.

## Limitations
- Single laptop with 8 GB RAM; only one machine tested.
- Run-to-run noise from laptop power and thermal behavior and background apps.
- N up to 100,000 and M = 200, not millions of scenarios, because each
  scenario costs M x T random numbers.
- Bottleneck causes (hyper-threading, clock speed, load imbalance) were not
  measured separately.
- Schedule differences are within measurement noise.

## Future Improvements
Test smaller N to find a crossover; larger M; chunk-size tuning for
dynamic/guided; hardware counters to identify bottlenecks; testing on a
machine with more physical cores.

## Team Workflow
Each member works on a separate branch and pushes to GitHub.
Do not commit executables or build folders.

## Required Deliverables
- Project report
- Sequential source code (sequential/)
- Parallel source code (parallel/)
- Input/test data (generated from a fixed seed; data/ holds a sample)
- Performance results and graphs (results/)
- README with compilation and execution instructions
- LLM Usage Log (report/)
- Individual contribution statement

## Important
All team members must understand the overall algorithm, parallelization
strategy, implementation, and performance results for the final
presentation/viva.