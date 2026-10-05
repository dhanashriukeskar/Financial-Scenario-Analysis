// parallel/parallel.cpp
// OpenMP evaluation of N financial scenarios.
//
// Usage:  ./parallel <N> <M> <threads> <schedule> [seed] [--out results.csv]
//   N        number of scenarios
//   M        Monte Carlo paths per scenario
//   threads  number of OpenMP threads (1 .. max available)
//   schedule static | dynamic | guided
//   seed     base random seed (default 12345, must match the sequential run)
//   --out    optional per-scenario CSV for comparison with the sequential CSV
//
// Build:  g++ -O2 -std=c++17 -Wall -Wextra -fopenmp parallel/parallel.cpp -o parallel

#include <omp.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../common/scenario.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <N> <M> <threads> <schedule> [seed] [--out results.csv]\n"
              << "  N         number of scenarios (> 0)\n"
              << "  M         Monte Carlo paths per scenario (> 0)\n"
              << "  threads   OpenMP threads (> 0)\n"
              << "  schedule  static | dynamic | guided\n"
              << "  seed      base random seed (default 12345)\n";
}

static bool parse_positive(const char* text, long long& value) {
    char* end = nullptr;
    value = std::strtoll(text, &end, 10);
    return end != text && *end == '\0' && value > 0;
}

int main(int argc, char* argv[]) {
    // ---------- 1. Read command-line arguments ----------
    if (argc < 5) { print_usage(argv[0]); return 1; }

    long long N = 0, M = 0, threads = 0;
    uint64_t seed = 12345;
    std::string schedule_name = argv[4];
    std::string out_file;

    if (!parse_positive(argv[1], N))       { std::cerr << "Error: N must be a positive integer\n"; return 1; }
    if (!parse_positive(argv[2], M))       { std::cerr << "Error: M must be a positive integer\n"; return 1; }
    if (!parse_positive(argv[3], threads)) { std::cerr << "Error: threads must be a positive integer\n"; return 1; }
    if (N > 100000000LL || M > 100000000LL) { std::cerr << "Error: N or M too large\n"; return 1; }
    if (threads > 256) { std::cerr << "Error: threads too large\n"; return 1; }
    if (schedule_name != "static" && schedule_name != "dynamic" && schedule_name != "guided") {
        std::cerr << "Error: schedule must be static, dynamic or guided\n";
        return 1;
    }

    for (int i = 5; i < argc; i++) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else {
            long long sv = 0;
            if (!parse_positive(argv[i], sv)) { print_usage(argv[0]); return 1; }
            seed = static_cast<uint64_t>(sv);
        }
    }

    // ---------- 2. Generate and validate scenarios (not timed) ----------
    // Same generator, same seed as the sequential version => same dataset.
    std::vector<Scenario> scenarios = generate_scenarios(N, seed);
    for (const Scenario& s : scenarios) {
        std::string error;
        if (!validate_scenario(s, error)) {
            std::cerr << "Invalid scenario " << s.id << ": " << error << "\n";
            return 1;
        }
    }
    std::vector<Result> results(static_cast<size_t>(N));

    // ---------- 3. Set the schedule ----------
    // schedule(runtime) reads the schedule from this call, so one loop
    // supports all three strategies without duplicating code.
    omp_set_num_threads(static_cast<int>(threads));
    if (schedule_name == "static")       omp_set_schedule(omp_sched_static, 0);
    else if (schedule_name == "dynamic") omp_set_schedule(omp_sched_dynamic, 1);
    else                                 omp_set_schedule(omp_sched_guided, 1);

    const int M_int = static_cast<int>(M);
    const long long n_total = N;

    // ---------- 4. Evaluate every scenario in parallel (timed region) ----------
    auto t_start = std::chrono::steady_clock::now();

    // Safe to parallelize: iteration i reads only scenarios[i] and writes only
    // results[i]. evaluate_scenario() uses its own RNG seeded with (seed + id),
    // so no data is shared between iterations. No critical section is needed.
    #pragma omp parallel for schedule(runtime)
    for (long long i = 0; i < n_total; i++) {
        results[static_cast<size_t>(i)] =
            evaluate_scenario(scenarios[static_cast<size_t>(i)], M_int, seed);
    }

    auto t_end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t_end - t_start).count();

    // ---------- 5. Summarise (not timed) ----------
    // Summed sequentially in index order, exactly like the sequential program,
    // so the totals match bit for bit. (See explanation below.)
    long long count_class[3] = {0, 0, 0};
    double sum_score = 0.0, sum_final = 0.0;
    for (const Result& r : results) {
        count_class[r.cls]++;
        sum_score += r.score;
        sum_final += r.mean_real_final;
    }

    std::printf("Parallel evaluation\n");
    std::printf("N (scenarios)      : %lld\n", N);
    std::printf("M (paths/scenario) : %lld\n", M);
    std::printf("Threads            : %lld\n", threads);
    std::printf("Schedule           : %s\n", schedule_name.c_str());
    std::printf("Seed               : %llu\n", static_cast<unsigned long long>(seed));
    std::printf("RECOMMENDED        : %lld\n", count_class[RECOMMENDED]);
    std::printf("ACCEPTABLE         : %lld\n", count_class[ACCEPTABLE]);
    std::printf("REJECTED           : %lld\n", count_class[REJECTED]);
    std::printf("Mean score         : %.10f\n", sum_score / N);
    std::printf("Sum of mean finals : %.6f\n", sum_final);
    std::printf("Evaluation time (s): %.6f\n", seconds);

    // ---------- 6. Optional per-scenario CSV ----------
    if (!out_file.empty()) {
        std::FILE* f = std::fopen(out_file.c_str(), "w");
        if (!f) { std::cerr << "Error: cannot open output file " << out_file << "\n"; return 1; }
        std::fprintf(f, "id,mean_real_final,profit,roi,annual_real_return,prob_loss,score,class\n");
        for (long long i = 0; i < N; i++) {
            const Result& r = results[static_cast<size_t>(i)];
            std::fprintf(f, "%lld,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%s\n", i,
                         r.mean_real_final, r.profit, r.roi, r.annual_real_return,
                         r.prob_loss, r.score, class_name(r.cls));
        }
        std::fclose(f);
        std::printf("Wrote per-scenario results to %s\n", out_file.c_str());
    }
    return 0;
}