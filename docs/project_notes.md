# Project Notes - Q19 Financial Scenario Analysis

Status: DRAFT v1 - awaiting approval from all 4 team members.

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

    generate N scenarios from seed
    start timer
    for i = 0 .. N-1:
        results[i] = evaluate_scenario(scenarios[i], M)
    stop timer
    count scenarios per class
    print summary

evaluate_scenario(s, M):
    create a random generator seeded with (base_seed + s.id)   <- local to the call
    for each of M paths: simulate T years, record real_final
    compute the statistics in section 4 and classify

## 7. Complexity

- Time: O(N * M * T)
- Space: O(N) for scenarios and results (O(M) temporary per scenario)

## 8. Why scenarios are independent (to be confirmed by Person 2)

- Iteration i reads only scenarios[i] and writes only results[i].
- Each scenario uses its own random generator seeded from its id, so the
  result does not depend on thread count, thread order or scheduling.
- No shared variable is modified inside the loop.

Caveat: std::normal_distribution is implementation-defined, so results are
guaranteed identical only for the same compiler and machine. Sequential and
parallel comparisons must use the same build setup.

## 9. Program interface (our decision)

    ./sequential <N> <M> [seed] [--out results.csv]

- Scenarios are generated inside the program from the seed (no large input files).
- Printed output: N, M, seed, counts per class, mean score, evaluation time (seconds).
- Optional per-scenario CSV: id, mean_real_final, profit, ROI, annual_real_return,
  prob_loss, score, class.
- Timing covers only the evaluation loop (std::chrono::steady_clock).

## 10. Open items

- M (paths per scenario): to be chosen by measurement so the largest N
  finishes in a reasonable time.
- Input sizes N: to be chosen after measuring (starting guess 1k to 1M).
- Thread counts: 1, 2, 4, 8 (to be confirmed on the machine used for results).

## Approvals

- [ ] Person 1 - Dhanashri
- [ ] Person 2
- [ ] Person 3
- [ ] Person 4