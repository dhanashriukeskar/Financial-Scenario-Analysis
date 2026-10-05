// benchmark/benchmark.cpp
// Runs the performance experiments and writes a CSV for graphing.
//
// Usage:  ./benchmark <experiment> [M] [repeats] [out.csv]
//   experiment : scale | threads | schedule | all
//   M          : Monte Carlo paths per scenario (default 1000)
//   repeats    : runs per configuration, median is reported (default 5)
//   out.csv    : output file (default results/benchmark_<experiment>.csv)
//
// Build:  g++ -O2 -std=c++17 -Wall -Wextra -fopenmp benchmark/benchmark.cpp -o benchmark
//
// TIMED: only the loop that evaluates all scenarios.
// NOT TIMED: scenario generation, validation, allocation, summary, printing.

#include <omp.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../common/scenario.h"

static const uint64_t SEED = 12345;

static bool parse_positive(const char* text, long long& value) {
    char* end = nullptr;
    value = std::strtoll(text, &end, 10);
    return end != text && *end == '\0' && value > 0;
}

static void set_schedule(const std::string& name) {
    if (name == "static")       omp_set_schedule(omp_sched_static, 0);
    else if (name == "dynamic") omp_set_schedule(omp_sched_dynamic, 1);
    else                        omp_set_schedule(omp_sched_guided, 1);
}

// Time ONE sequential pass. Returns seconds. 'checksum' stops the compiler
// from removing the work and lets us confirm the runs agree.
static double time_sequential(const std::vector<Scenario>& sc, std::vector<Result>& res,
                              int M, double& checksum) {
    const long long n = static_cast<long long>(sc.size());
    auto t0 = std::chrono::steady_clock::now();
    for (long long i = 0; i < n; i++)
        res[static_cast<size_t>(i)] = evaluate_scenario(sc[static_cast<size_t>(i)], M, SEED);
    auto t1 = std::chrono::steady_clock::now();
    checksum = 0.0;
    for (const Result& r : res) checksum += r.mean_real_final;
    return std::chrono::duration<double>(t1 - t0).count();
}

// Time ONE parallel pass.
static double time_parallel(const std::vector<Scenario>& sc, std::vector<Result>& res,
                            int M, int threads, const std::string& schedule, double& checksum) {
    const long long n = static_cast<long long>(sc.size());
    omp_set_num_threads(threads);
    set_schedule(schedule);
    auto t0 = std::chrono::steady_clock::now();
    #pragma omp parallel for schedule(runtime)
    for (long long i = 0; i < n; i++)
        res[static_cast<size_t>(i)] = evaluate_scenario(sc[static_cast<size_t>(i)], M, SEED);
    auto t1 = std::chrono::steady_clock::now();
    checksum = 0.0;
    for (const Result& r : res) checksum += r.mean_real_final;
    return std::chrono::duration<double>(t1 - t0).count();
}

static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// Counts accepted-type results for the CSV.
static void count_classes(const std::vector<Result>& res, long long c[3]) {
    c[0] = c[1] = c[2] = 0;
    for (const Result& r : res) c[r.cls]++;
}

struct Row {
    std::string experiment;
    long long N;
    std::string mode;
    int threads;
    std::string schedule;
    double median_time;
    double speedup;
    double efficiency_pct;
    long long recommended, acceptable, rejected;
};

// Measure one configuration: 1 warm-up run (discarded) + 'repeats' timed runs.
static double measure_sequential(const std::vector<Scenario>& sc, int M, int repeats,
                                 long long classes[3]) {
    std::vector<Result> res(sc.size());
    double cs = 0.0;
    time_sequential(sc, res, M, cs);                 // warm-up
    std::vector<double> times;
    for (int k = 0; k < repeats; k++) times.push_back(time_sequential(sc, res, M, cs));
    count_classes(res, classes);
    return median(times);
}

static double measure_parallel(const std::vector<Scenario>& sc, int M, int repeats, int threads,
                               const std::string& schedule, long long classes[3]) {
    std::vector<Result> res(sc.size());
    double cs = 0.0;
    time_parallel(sc, res, M, threads, schedule, cs);   // warm-up
    std::vector<double> times;
    for (int k = 0; k < repeats; k++)
        times.push_back(time_parallel(sc, res, M, threads, schedule, cs));
    count_classes(res, classes);
    return median(times);
}

static void write_csv(const std::string& path, const std::vector<Row>& rows) {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) { std::cerr << "Error: cannot open " << path << " (does the folder exist?)\n"; return; }
    std::fprintf(f, "Experiment,Scenarios,Mode,Threads,Schedule,MedianTime,Speedup,EfficiencyPct,"
                    "Recommended,Acceptable,Rejected\n");
    for (const Row& r : rows)
        std::fprintf(f, "%s,%lld,%s,%d,%s,%.6f,%.4f,%.2f,%lld,%lld,%lld\n", r.experiment.c_str(),
                     r.N, r.mode.c_str(), r.threads, r.schedule.c_str(), r.median_time, r.speedup,
                     r.efficiency_pct, r.recommended, r.acceptable, r.rejected);
    std::fclose(f);
    std::printf("Wrote %zu rows to %s\n", rows.size(), path.c_str());
}

static std::vector<int> usable_threads() {
    int mx = omp_get_max_threads();
    std::vector<int> v;
    for (int t : {1, 2, 4, 8, 16}) if (t <= mx) v.push_back(t);
    return v;
}

// Experiment 1: execution time vs number of scenarios (sequential and parallel).
static void run_scale(int M, int repeats, std::vector<Row>& rows) {
    // Edit this list to match your RAM and patience. With M = 1000 the
    // largest sizes take a long time; reduce M or the list if needed.
    const std::vector<long long> sizes = {100, 1000, 5000, 10000, 50000, 100000};
    const int threads = omp_get_max_threads();
    const std::string schedule = "static";
    for (long long N : sizes) {
        std::printf("[scale] N = %lld ...\n", N);
        std::vector<Scenario> sc = generate_scenarios(N, SEED);   // not timed
        long long c[3];
        double ts = measure_sequential(sc, M, repeats, c);
        rows.push_back({"scale", N, "Sequential", 1, "NA", ts, 1.0, 100.0, c[2], c[1], c[0]});
        double tp = measure_parallel(sc, M, repeats, threads, schedule, c);
        double sp = ts / tp;
        rows.push_back({"scale", N, "Parallel", threads, schedule, tp, sp, 100.0 * sp / threads,
                        c[2], c[1], c[0]});
    }
}

// Experiment 2: execution time, speedup, efficiency vs number of threads.
static void run_threads(int M, int repeats, std::vector<Row>& rows) {
    const long long N = 10000;     // fixed workload; change if too slow/fast
    std::printf("[threads] N = %lld\n", N);
    std::vector<Scenario> sc = generate_scenarios(N, SEED);
    long long c[3];
    double ts = measure_sequential(sc, M, repeats, c);
    rows.push_back({"threads", N, "Sequential", 1, "NA", ts, 1.0, 100.0, c[2], c[1], c[0]});
    for (int t : usable_threads()) {
        double tp = measure_parallel(sc, M, repeats, t, "static", c);
        double sp = ts / tp;
        rows.push_back({"threads", N, "Parallel", t, "static", tp, sp, 100.0 * sp / t,
                        c[2], c[1], c[0]});
    }
}

// Experiment 3: static vs dynamic vs guided at a fixed thread count.
static void run_schedule(int M, int repeats, std::vector<Row>& rows) {
    const long long N = 10000;
    const int threads = omp_get_max_threads();
    std::printf("[schedule] N = %lld, threads = %d\n", N, threads);
    std::vector<Scenario> sc = generate_scenarios(N, SEED);
    long long c[3];
    double ts = measure_sequential(sc, M, repeats, c);
    for (const char* s : {"static", "dynamic", "guided"}) {
        double tp = measure_parallel(sc, M, repeats, threads, s, c);
        double sp = ts / tp;
        rows.push_back({"schedule", N, "Parallel", threads, s, tp, sp, 100.0 * sp / threads,
                        c[2], c[1], c[0]});
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <scale|threads|schedule|all> [M] [repeats] [out.csv]\n";
        return 1;
    }
    const std::string exp = argv[1];
    if (exp != "scale" && exp != "threads" && exp != "schedule" && exp != "all") {
        std::cerr << "Error: experiment must be scale, threads, schedule or all\n";
        return 1;
    }
    long long M = 1000, repeats = 5;
    if (argc >= 3 && !parse_positive(argv[2], M)) { std::cerr << "Error: bad M\n"; return 1; }
    if (argc >= 4 && !parse_positive(argv[3], repeats)) { std::cerr << "Error: bad repeats\n"; return 1; }
    if (M > 1000000LL || repeats > 50) { std::cerr << "Error: M or repeats too large\n"; return 1; }
    const std::string out = (argc >= 5) ? argv[4] : "results/benchmark_" + exp + ".csv";

    std::printf("Max threads available: %d, M = %lld, repeats = %lld\n",
                omp_get_max_threads(), M, repeats);

    std::vector<Row> rows;
    if (exp == "scale"    || exp == "all") run_scale(static_cast<int>(M), static_cast<int>(repeats), rows);
    if (exp == "threads"  || exp == "all") run_threads(static_cast<int>(M), static_cast<int>(repeats), rows);
    if (exp == "schedule" || exp == "all") run_schedule(static_cast<int>(M), static_cast<int>(repeats), rows);
    write_csv(out, rows);
    return 0;
}