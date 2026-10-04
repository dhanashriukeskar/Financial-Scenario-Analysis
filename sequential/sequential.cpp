// sequential/sequential.cpp
// Sequential evaluation of N financial scenarios (Q19 Financial Scenario Analysis).
//
// Usage:  ./sequential <N> <M> [seed] [--out results.csv]
//   N    number of scenarios
//   M    Monte Carlo paths per scenario
//   seed base random seed (default 12345)
//   --out  optional per-scenario CSV (used to compare with the parallel version)
//
// Build:  g++ -O2 -std=c++17 -Wall -Wextra sequential/sequential.cpp -o sequential

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../common/scenario.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <N> <M> [seed] [--out results.csv]\n"
              << "  N    number of scenarios (> 0)\n"
              << "  M    Monte Carlo paths per scenario (> 0)\n"
              << "  seed base random seed (default 12345)\n";
}

// Parse a positive integer. Returns false on bad input.
static bool parse_positive(const char* text, long long& value) {
    char* end = nullptr;
    value = std::strtoll(text, &end, 10);
    return end != text && *end == '\0' && value > 0;
}

int main(int argc, char* argv[]) {
    // ---------- 1. Read command-line arguments ----------
    if (argc < 3) { print_usage(argv[0]); return 1; }

    long long N = 0, M = 0;
    uint64_t seed = 12345;
    std::string out_file;

    if (!parse_positive(argv[1], N)) { std::cerr << "Error: N must be a positive integer\n"; return 1; }
    if (!parse_positive(argv[2], M)) { std::cerr << "Error: M must be a positive integer\n"; return 1; }
    if (N > 100000000LL || M > 100000000LL) { std::cerr << "Error: N or M too large\n"; return 1; }

    for (int i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else {
            long long sv = 0;
            if (!parse_positive(argv[i], sv)) { print_usage(argv[0]); return 1; }
            seed = static_cast<uint64_t>(sv);
        }
    }

    // ---------- 2. Generate and validate scenarios (not timed) ----------
    std::vector<Scenario> scenarios = generate_scenarios(N, seed);
    for (const Scenario& s : scenarios) {
        std::string error;
        if (!validate_scenario(s, error)) {
            std::cerr << "Invalid scenario " << s.id << ": " << error << "\n";
            return 1;
        }
    }
    std::vector<Result> results(static_cast<size_t>(N));

    // ---------- 3. Evaluate every scenario (timed region) ----------
    auto t_start = std::chrono::steady_clock::now();

    for (long long i = 0; i < N; i++) {
        results[static_cast<size_t>(i)] =
            evaluate_scenario(scenarios[static_cast<size_t>(i)], static_cast<int>(M), seed);
    }

    auto t_end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t_end - t_start).count();

    // ---------- 4. Summarise (not timed) ----------
    long long count_class[3] = {0, 0, 0};
    double sum_score = 0.0, sum_final = 0.0;
    for (const Result& r : results) {
        count_class[r.cls]++;
        sum_score += r.score;
        sum_final += r.mean_real_final;
    }

    std::printf("Sequential evaluation\n");
    std::printf("N (scenarios)      : %lld\n", N);
    std::printf("M (paths/scenario) : %lld\n", M);
    std::printf("Seed               : %llu\n", static_cast<unsigned long long>(seed));
    std::printf("RECOMMENDED        : %lld\n", count_class[RECOMMENDED]);
    std::printf("ACCEPTABLE         : %lld\n", count_class[ACCEPTABLE]);
    std::printf("REJECTED           : %lld\n", count_class[REJECTED]);
    std::printf("Mean score         : %.10f\n", sum_score / N);
    std::printf("Sum of mean finals : %.6f\n", sum_final);
    std::printf("Evaluation time (s): %.6f\n", seconds);

    // ---------- 5. Optional per-scenario CSV ----------
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