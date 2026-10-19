# Input / Test Data

No input file is needed. Scenarios are generated inside the program from a fixed
seed (default 12345), so the sequential and parallel programs always evaluate the
same scenarios. Dataset size is chosen with the command-line argument N.

`sample_scenarios.csv` is a sample of the per-scenario output (N = 10, M = 200,
seed 12345), created with:

    ./sequential 10 200 --out data/sample_scenarios.csv
