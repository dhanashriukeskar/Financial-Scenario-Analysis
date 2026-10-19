# Performance Results

Final measurements used in the report (median of repeated runs, M = 200):

- `benchmark_scale.csv`    time vs number of scenarios (8 threads, static)
- `benchmark_threads.csv`  time vs number of threads (N = 10,000, static)
- `benchmark_schedule.csv` static vs dynamic vs guided (N = 10,000, 8 threads)
- `graphs/`                graph1 to graph7 used in the report

Columns: Experiment, Scenarios, Mode, Threads, Schedule, MedianTime, Speedup,
EfficiencyPct, Recommended, Acceptable, Rejected.
