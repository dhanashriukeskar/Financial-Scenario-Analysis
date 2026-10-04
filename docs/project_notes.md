# Project Notes - Q19 Financial Scenario Analysis

Status: DRAFT v2 - sequential version implemented; awaiting approval from all 4 team members.

## 1. Problem (official)

"Develop a computational system for evaluating a large number of financial
scenarios according to specified criteria. Investigate how execution time
changes with the number of scenarios."

## 2. Official requirements vs our design decisions

| Item | Source |
|---|---|
| Sequential version and parallel version (OpenMP and/or MPI and/or CUDA) | Official |
| Justify the parallel model chosen | Official |
| Verify parallel output against sequential | Official |
| Execution time, speedup, efficiency, effect of threads, effect of input size, bottlenecks, graphs/tables | Official |
| Scenario fields, formulas, thresholds, input sizes, thread counts | Our decision |
| Monte Carlo evaluation of each scenario | Our decision |
| Team roles and Git workflow | Our decision |

## 3. Scenario definition (our decision)

| Field | Meaning | Generator range |
|---|---|---|
| id | scenario number | 0 to N-1 |
| P | initial investment | 10,000 to 1,000,000 |
| r | expected annual return | 2% to 20% |
| sigma | risk: volatility of yearly log-returns | 5% to 40% |
| T | duration in whole years | 1 to 30 |
| infl | annual inflation | 2% to 8% |

Valid input: P > 0, sigma >= 0, T >= 1 (integer), r > -100%, infl > -100%.
Invalid scenarios are rejected with an error message.

## 4. Evaluation of one scenario (our decision)

Each scenario is simulated with M random paths.

For each path:
1. V = P
2. For each year 1..T: draw Z from N(0,1), then
   V = V * exp( (ln(1+r) - sigma^2/2) + sigma * Z )
   (the -sigma^2/2 term makes the expected yearly growth exactly 1+r)
3. real_final = V / (1+infl)^T

Over the M paths:
- mean_real_final = average of real_final
- profit = mean_real_final - P
- ROI = profit / P
- annual_real_return = (mean_real_final / P)^(1/T) - 1
- prob_loss = fraction of paths with real_final < P
- score = ROI / stdev(real_final / P)   (Sharpe-like risk-adjusted score)

Closed-form check (used for testing): with sigma = 0,
mean_real_final = P * ((1+r)/(1+infl))^T.

## 5. Classification criteria (our decision, thresholds are constants)

- RECOMMENDED: prob_loss <= 0.20 AND annual_real_return >= 0.03
- ACCEPTABLE:  prob_loss <= 0.40 AND annual_real_return >= 0
- REJECTED:    otherwise

## 6. Sequential algorithm

    generate N scenarios from seed          (not timed)
    validate every scenario                 (not timed)
    start timer
    for i = 0 .. N-1:
        results[i] = evaluate_scenario(scenarios[i], M, seed)
    stop timer
    count scenarios per class, print summary

evaluate_scenario(s, M, seed):
    create a random generator seeded with (seed + s.id)   <- local to the call
    for each of M paths: simulate T years, compute real_final
    compute the statistics in section 4 and classify

## 7. Complexity

- Time: O(N * M * T)
- Space: O(N) for scenarios and results (O(1) temporary per scenario, since
  running statistics are updated per path and paths are not stored)

## 8. Why scenarios are independent (to be confirmed by Person 2)

- Iteration i reads only scenarios[i] and writes only results[i].
- Each scenario uses its own random generator seeded from its id, so the
  result does not depend on thread count, thread order or scheduling.
- evaluate_scenario() uses no global or shared variables.

Caveat: std::normal_distribution is implementation-defined, so results are
guaranteed identical only for the same compiler and library. Sequential and
parallel comparisons must use the same build setup.

## 9. Program interface and files (our decision)

Files:
- common/scenario.h        shared struct, generator, validation, evaluate_scenario(), classification
- sequential/sequential.cpp   sequential driver (reference)
- sequential/test_sequential.cpp   basic correctness tests (17 checks)
- parallel/parallel.cpp    parallel driver (must call the same evaluate_scenario())

Build and run (from the repository root):

    g++ -O2 -std=c++17 -Wall -Wextra sequential/sequential.cpp -o seq_run.exe
    .\seq_run.exe <N> <M> [seed] [--out results\file.csv]

    g++ -O2 -std=c++17 -Wall -Wextra sequential/test_sequential.cpp -o test_sequential.exe
    .\test_sequential.exe

- Scenarios are generated inside the program from the seed (no large input files).
- Default seed is 12345.
- Printed output: N, M, seed, counts per class, mean score, sum of mean finals,
  evaluation time (seconds).
- Optional per-scenario CSV: id, mean_real_final, profit, roi, annual_real_return,
  prob_loss, score, class (17 significant digits, so files can be compared exactly).
- Timing covers only the evaluation loop (std::chrono::steady_clock).

## 10. Experiment settings

- M (paths per scenario): M = 100, chosen from measured sequential timings.
  Time per path stayed almost constant for M = 50 to 500 (N = 10,000), and
  one run at N = 1,000,000 took about 52 s on the author's machine
  (Intel i5-10210U, 4 cores / 8 threads, 8 GB RAM, g++ 13.2 MinGW).
  To be confirmed by the team.
- Input sizes N: starting guess 1,000 to 1,000,000; final values to be chosen
  from measurements.
- Thread counts: 1, 2, 4, 8 (to be confirmed on the machine used for results).
- Final performance results must come from repeated runs on one chosen machine
  (Person 3); the early timings above are single measurements only.

## 11. Implementation notes (sequential version)

- Score is undefined when stdev = 0 (risk-free scenario): we set score = 0.
- A path counts as a loss only if real_final < P * (1 - 1e-12), so floating-point
  rounding cannot mark an exact break-even scenario as a loss.
- Adding the yearly log-returns and taking one exp per path is mathematically
  identical to multiplying the yearly factors in section 4, and cheaper.
- Evaluation code is shared in common/scenario.h; the parallel version must
  call the same evaluate_scenario() function.
- Monte Carlo with M = 100 gives noisy per-scenario statistics, so individual
  classifications are approximate. Sequential and parallel runs use the same
  random streams, so they should produce identical results.

## Approvals

- [ ] Person 1 - Dhanashri
- [ ] Person 2
- [ ] Person 3
- [ ] Person 4