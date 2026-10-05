// verify/verify.cpp
// Correctness check: runs the sequential evaluation and the OpenMP evaluation
// on the SAME dataset and compares them.
//
// Usage:  ./verify <N> <M> [seed]
//
// Build:  g++ -O2 -std=c++17 -Wall -Wextra -fopenmp verify/verify.cpp -o verify

#include <omp.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../common/scenario.h"

// Aggregate numbers over all scenarios.
struct Totals {
    long long count[3] = {0, 0, 0};   // REJECTED, ACCEPTABLE, RECOMMENDED
    double sum_score = 0.0;
    double sum_final = 0.0;
};

static bool parse_positive(const char* text, long long& value) {
    char* end = nullptr;
    value = std::strtoll(text, &end, 10);
    return end != text && *end == '\0' && value > 0;
}

// Reference summary: adds the results in index order (same as sequential.cpp).
static Totals summarise_in_order(const std::vector<Result>& results) {
    Totals t;
    for (const Result& r : results) {
        t.count[r.cls]++;
        t.sum_score += r.score;
        t.sum_final += r.mean_real_final;
    }
    return t;
}

// Exact comparison of one scenario's result. Allowed to use == because every
// scenario has its own RNG, so the arithmetic is identical in both versions.
static bool same_result(const Result& a, const Result& b) {
    return a.mean_real_final == b.mean_real_final && a.profit == b.profit &&
           a.roi == b.roi && a.annual_real_return == b.annual_real_return &&
           a.prob_loss == b.prob_loss && a.score == b.score && a.cls == b.cls;
}

// Relative tolerance comparison (see explanation in the README / report).
static bool close_enough(double a, double b, double tol) {
    return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b));
}

// Parallel evaluation that ALSO computes the totals with an OpenMP reduction.
// Each thread keeps private copies of the five accumulators; OpenMP combines
// them at the end of the loop. No critical section, no race condition.
static Totals evaluate_parallel_reduction(const std::vector<Scenario>& scenarios,
                                          std::vector<Result>& results, int M,
                                          uint64_t seed, int threads,
                                          const std::string& schedule) {
    omp_set_num_threads(threads);
    if (schedule == "static")       omp_set_schedule(omp_sched_static, 0);
    else if (schedule == "dynamic") omp_set_schedule(omp_sched_dynamic, 1);
    else                            omp_set_schedule(omp_sched_guided, 1);

    long long rec = 0, acc = 0, rej = 0;
    double sum_score = 0.0, sum_final = 0.0;
    const long long n = static_cast<long long>(scenarios.size());

    #pragma omp parallel for schedule(runtime) reduction(+ : rec, acc, rej, sum_score, sum_final)
    for (long long i = 0; i < n; i++) {
        Result r = evaluate_scenario(scenarios[static_cast<size_t>(i)], M, seed);
        results[static_cast<size_t>(i)] = r;
        if (r.cls == RECOMMENDED)      rec++;
        else if (r.cls == ACCEPTABLE)  acc++;
        else                           rej++;
        sum_score += r.score;
        sum_final += r.mean_real_final;
    }

    Totals t;
    t.count[RECOMMENDED] = rec;
    t.count[ACCEPTABLE]  = acc;
    t.count[REJECTED]    = rej;
    t.sum_score = sum_score;
    t.sum_final = sum_final;
    return t;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <N> <M> [seed]\n";
        return 1;
    }
    long long N = 0, M = 0, sv = 12345;
    if (!parse_positive(argv[1], N) || !parse_positive(argv[2], M) ||
        (argc == 4 && !parse_positive(argv[3], sv))) {
        std::cerr << "Error: N, M and seed must be positive integers\n";
        return 1;
    }
    if (N > 10000000LL || M > 1000000LL) { std::cerr << "Error: N or M too large\n"; return 1; }
    const uint64_t seed = static_cast<uint64_t>(sv);
    const int M_int = static_cast<int>(M);

    // ---- One dataset, shared by every run below ----
    std::vector<Scenario> scenarios = generate_scenarios(N, seed);
    for (const Scenario& s : scenarios) {
        std::string error;
        if (!validate_scenario(s, error)) {
            std::cerr << "Invalid scenario " << s.id << ": " << error << "\n";
            return 1;
        }
    }

    // ---- Reference: sequential evaluation ----
    std::vector<Result> seq_results(static_cast<size_t>(N));
    for (long long i = 0; i < N; i++) {
        seq_results[static_cast<size_t>(i)] =
            evaluate_scenario(scenarios[static_cast<size_t>(i)], M_int, seed);
    }
    const Totals seq = summarise_in_order(seq_results);

    // ---- Thread counts: 1, 2, 4, 8 but never more than the CPU offers ----
    const int max_threads = omp_get_max_threads();
    std::vector<int> thread_counts;
    for (int t : {1, 2, 4, 8}) if (t <= max_threads) thread_counts.push_back(t);
    const char* schedules[3] = {"static", "dynamic", "guided"};

    const double TOL = 1e-9;   // relative tolerance for double sums

    std::printf("CORRECTNESS CHECK\n");
    std::printf("N = %lld, M = %lld, seed = %llu, max threads = %d, tolerance = %g\n\n",
                N, M, static_cast<unsigned long long>(seed), max_threads, TOL);
    std::printf("Reference (sequential): RECOMMENDED=%lld ACCEPTABLE=%lld REJECTED=%lld\n\n",
                seq.count[RECOMMENDED], seq.count[ACCEPTABLE], seq.count[REJECTED]);
    std::printf("%-7s %-8s %-8s %-12s %-10s %-10s %-8s\n", "Threads", "Schedule", "Counts",
                "Individual", "SumScore", "SumFinal", "Result");

    int failed = 0;
    double worst_rel_diff = 0.0;

    for (int threads : thread_counts) {
        for (const char* sched : schedules) {
            std::vector<Result> par_results(static_cast<size_t>(N));
            Totals par = evaluate_parallel_reduction(scenarios, par_results, M_int, seed,
                                                     threads, sched);

            bool counts_ok = par.count[0] == seq.count[0] && par.count[1] == seq.count[1] &&
                             par.count[2] == seq.count[2];

            long long mismatches = 0;
            for (long long i = 0; i < N; i++) {
                if (!same_result(seq_results[static_cast<size_t>(i)],
                                 par_results[static_cast<size_t>(i)])) mismatches++;
            }
            bool individual_ok = (mismatches == 0);

            bool score_ok = close_enough(par.sum_score, seq.sum_score, TOL);
            bool final_ok = close_enough(par.sum_final, seq.sum_final, TOL);

            double rel = std::fabs(par.sum_final - seq.sum_final) /
                         std::max(1.0, std::fabs(seq.sum_final));
            worst_rel_diff = std::max(worst_rel_diff, rel);

            bool all_ok = counts_ok && individual_ok && score_ok && final_ok;
            if (!all_ok) failed++;

            std::printf("%-7d %-8s %-8s %-12s %-10s %-10s %-8s\n", threads, sched,
                        counts_ok ? "MATCH" : "DIFFER", individual_ok ? "MATCH" : "DIFFER",
                        score_ok ? "MATCH" : "DIFFER", final_ok ? "MATCH" : "DIFFER",
                        all_ok ? "PASS" : "FAIL");
        }
    }

    std::printf("\nLargest relative difference in sum of finals (reduction vs in-order): %.3e\n",
                worst_rel_diff);
    std::printf("Configurations tested: %zu, failed: %d\n",
                thread_counts.size() * 3, failed);
    std::printf(failed == 0 ? "OVERALL: PASS\n" : "OVERALL: FAIL\n");
    return failed == 0 ? 0 : 1;
}