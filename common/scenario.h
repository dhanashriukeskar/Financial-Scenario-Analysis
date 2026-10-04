// common/scenario.h
// Shared by sequential.cpp and parallel.cpp so both versions use IDENTICAL
// evaluation code. The parallel version only changes the loop in main().
//
// Everything here follows docs/project_notes.md (sections 3, 4, 5).
#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// ---------- Section 3: scenario definition (our design decision) ----------
struct Scenario {
    int    id;     // scenario number 0..N-1
    double P;      // initial investment
    double r;      // expected annual return (0.08 = 8%)
    double sigma;  // risk: volatility of yearly log-returns (0.20 = 20%)
    int    T;      // duration in whole years
    double infl;   // annual inflation (0.05 = 5%)
};

// ---------- Section 5: classification ----------
enum ScenarioClass { REJECTED = 0, ACCEPTABLE = 1, RECOMMENDED = 2 };

// Thresholds are constants so the team can change them in one place.
const double REC_MAX_PROB_LOSS = 0.20;   // RECOMMENDED: prob_loss <= 0.20
const double REC_MIN_ANNUAL    = 0.03;   //   and annual_real_return >= 3%
const double ACC_MAX_PROB_LOSS = 0.40;   // ACCEPTABLE:  prob_loss <= 0.40
const double ACC_MIN_ANNUAL    = 0.0;    //   and annual_real_return >= 0

// A path counts as a loss only if real final value is below P by more than
// this tiny tolerance. Without it, floating-point rounding could mark an
// exact break-even scenario as a "loss".
const double LOSS_TOLERANCE = 1e-12;

// ---------- Section 4: result of evaluating one scenario ----------
struct Result {
    double mean_real_final;      // average inflation-adjusted final value
    double profit;               // mean_real_final - P
    double roi;                  // profit / P
    double annual_real_return;   // (mean_real_final / P)^(1/T) - 1
    double prob_loss;            // fraction of paths with real_final < P
    double score;                // roi / stdev(real_final / P); 0 if stdev == 0
    int    cls;                  // ScenarioClass
};

// ---------- Input validation (section 3) ----------
// Returns true if valid. Otherwise fills 'error' and returns false.
inline bool validate_scenario(const Scenario& s, std::string& error) {
    if (!(s.P > 0.0))     { error = "initial investment P must be > 0"; return false; }
    if (!(s.sigma >= 0.0)) { error = "risk sigma must be >= 0"; return false; }
    if (s.T < 1)          { error = "duration T must be >= 1 year"; return false; }
    if (!(s.r > -1.0))    { error = "expected return r must be > -100%"; return false; }
    if (!(s.infl > -1.0)) { error = "inflation must be > -100%"; return false; }
    return true;
}

// ---------- Scenario generator (reproducible from the seed) ----------
// Ranges are our design decision (section 3). Always run sequentially,
// in both versions, and it is NOT part of the timed region.
inline std::vector<Scenario> generate_scenarios(long long N, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_real_distribution<double> uP(10000.0, 1000000.0);
    std::uniform_real_distribution<double> uR(0.02, 0.20);
    std::uniform_real_distribution<double> uS(0.05, 0.40);
    std::uniform_int_distribution<int>     uT(1, 30);
    std::uniform_real_distribution<double> uI(0.02, 0.08);

    std::vector<Scenario> v(static_cast<size_t>(N));
    for (long long i = 0; i < N; i++) {
        Scenario s;
        s.id    = static_cast<int>(i);
        s.P     = uP(gen);
        s.r     = uR(gen);
        s.sigma = uS(gen);
        s.T     = uT(gen);
        s.infl  = uI(gen);
        v[static_cast<size_t>(i)] = s;
    }
    return v;
}

// ---------- Classification (section 5) ----------
inline int classify(double prob_loss, double annual_real_return) {
    if (prob_loss <= REC_MAX_PROB_LOSS && annual_real_return >= REC_MIN_ANNUAL)
        return RECOMMENDED;
    if (prob_loss <= ACC_MAX_PROB_LOSS && annual_real_return >= ACC_MIN_ANNUAL)
        return ACCEPTABLE;
    return REJECTED;
}

inline const char* class_name(int cls) {
    if (cls == RECOMMENDED) return "RECOMMENDED";
    if (cls == ACCEPTABLE)  return "ACCEPTABLE";
    return "REJECTED";
}

// ---------- Section 4: evaluate ONE scenario ----------
// Pure function: reads only its arguments, writes only local variables, and
// uses its OWN random generator seeded with (base_seed + id). So the result
// depends only on (scenario, M, base_seed), never on other scenarios, thread
// count, or execution order. This is what makes the parallel version safe.
inline Result evaluate_scenario(const Scenario& s, int M, uint64_t base_seed) {
    std::mt19937_64 gen(base_seed + static_cast<uint64_t>(s.id));
    std::normal_distribution<double> normal(0.0, 1.0);

    // Yearly log-return ~ Normal(drift, sigma). The -sigma^2/2 term makes the
    // expected yearly growth factor exactly (1 + r).
    const double drift = std::log(1.0 + s.r) - 0.5 * s.sigma * s.sigma;
    const double inflation_factor = std::pow(1.0 + s.infl, s.T);

    // Welford's method: running mean and variance of x = real_final / P.
    double mean_x = 0.0, m2 = 0.0;
    int count = 0, losses = 0;

    for (int m = 0; m < M; m++) {
        // Multiplying T yearly factors exp(z_1)...exp(z_T) equals
        // exp(z_1 + ... + z_T), so we add the yearly log-returns and take
        // one exp per path. Same result as section 4, fewer exp() calls.
        double log_growth = 0.0;
        for (int y = 0; y < s.T; y++) {
            log_growth += drift + s.sigma * normal(gen);
        }
        double x = std::exp(log_growth) / inflation_factor;  // real_final / P

        count++;
        double delta = x - mean_x;
        mean_x += delta / count;
        m2 += delta * (x - mean_x);

        if (x < 1.0 - LOSS_TOLERANCE) losses++;
    }

    Result res;
    res.mean_real_final    = mean_x * s.P;
    res.profit             = res.mean_real_final - s.P;
    res.roi                = res.profit / s.P;
    res.annual_real_return = std::pow(mean_x, 1.0 / s.T) - 1.0;
    res.prob_loss          = static_cast<double>(losses) / M;

    double stdev = (M > 1) ? std::sqrt(m2 / (M - 1)) : 0.0;
    // Score is undefined when stdev == 0 (risk-free case): we set it to 0.
    res.score = (stdev > 1e-12) ? res.roi / stdev : 0.0;

    res.cls = classify(res.prob_loss, res.annual_real_return);
    return res;
}