// sequential/test_sequential.cpp
// Basic correctness tests for the shared evaluation code.
//
// Build:  g++ -O2 -std=c++17 -Wall -Wextra sequential/test_sequential.cpp -o test_sequential
// Run:    ./test_sequential

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../common/scenario.h"

static int tests_run = 0, tests_failed = 0;

static void check(bool condition, const char* name) {
    tests_run++;
    if (condition) std::printf("PASS  %s\n", name);
    else { tests_failed++; std::printf("FAIL  %s\n", name); }
}

static bool close_rel(double a, double b, double tol) {
    return std::fabs(a - b) <= tol * std::fabs(b);
}

static Scenario make(int id, double P, double r, double sigma, int T, double infl) {
    Scenario s; s.id = id; s.P = P; s.r = r; s.sigma = sigma; s.T = T; s.infl = infl;
    return s;
}

int main() {
    const uint64_t seed = 12345;

    // 1. Zero risk: result must equal the closed form P*((1+r)/(1+infl))^T
    {
        Scenario cases[3] = { make(0, 100000, 0.10, 0.0, 5, 0.04),
                              make(1, 50000, 0.07, 0.0, 20, 0.03),
                              make(2, 250000, 0.15, 0.0, 30, 0.06) };
        bool ok = true;
        for (const Scenario& s : cases) {
            Result r = evaluate_scenario(s, 10, seed);
            double expected = s.P * std::pow((1 + s.r) / (1 + s.infl), s.T);
            if (!close_rel(r.mean_real_final, expected, 1e-9)) ok = false;
        }
        check(ok, "zero risk matches closed form");
    }

    // 2. Real growth zero (r == inflation, sigma == 0): ROI must be 0, no loss counted
    {
        Result r = evaluate_scenario(make(0, 100000, 0.05, 0.0, 10, 0.05), 10, seed);
        check(std::fabs(r.roi) < 1e-9 && r.prob_loss == 0.0, "r == inflation gives ROI 0 and no loss");
    }

    // 3. Statistical check: with sigma > 0 and large M, mean is close to closed form
    {
        Scenario s = make(7, 100000, 0.08, 0.20, 10, 0.05);
        Result r = evaluate_scenario(s, 20000, seed);
        double expected = s.P * std::pow((1 + s.r) / (1 + s.infl), s.T);
        check(close_rel(r.mean_real_final, expected, 0.03),
              "Monte Carlo mean within 3% of closed form (M = 20000)");
    }

    // 4. Determinism: same inputs give bit-identical results
    {
        Scenario s = make(3, 80000, 0.09, 0.25, 12, 0.04);
        Result a = evaluate_scenario(s, 500, seed);
        Result b = evaluate_scenario(s, 500, seed);
        check(a.mean_real_final == b.mean_real_final && a.score == b.score &&
              a.prob_loss == b.prob_loss, "same seed gives identical result");
    }

    // 5. Independence: result of a scenario does not depend on evaluation order
    {
        std::vector<Scenario> sc = generate_scenarios(50, seed);
        std::vector<Result> forward(50), backward(50);
        for (int i = 0; i < 50; i++) forward[i] = evaluate_scenario(sc[i], 200, seed);
        for (int i = 49; i >= 0; i--) backward[i] = evaluate_scenario(sc[i], 200, seed);
        bool same = true;
        for (int i = 0; i < 50; i++)
            if (forward[i].mean_real_final != backward[i].mean_real_final || forward[i].cls != backward[i].cls)
                same = false;
        check(same, "evaluation order does not change any result");
    }

    // 6. Generator is reproducible
    {
        std::vector<Scenario> a = generate_scenarios(20, seed), b = generate_scenarios(20, seed);
        bool same = true;
        for (int i = 0; i < 20; i++) if (a[i].P != b[i].P || a[i].T != b[i].T) same = false;
        check(same, "generator reproducible for the same seed");
    }

    // 7. Classification thresholds (risk-free cases so the outcome is certain)
    {
        Result rec = evaluate_scenario(make(0, 100000, 0.10, 0.0, 5, 0.04), 10, seed);  // ~5.8% real
        Result acc = evaluate_scenario(make(1, 100000, 0.05, 0.0, 5, 0.04), 10, seed);  // ~1.0% real
        Result rej = evaluate_scenario(make(2, 100000, 0.02, 0.0, 5, 0.05), 10, seed);  // negative real
        check(rec.cls == RECOMMENDED, "classification: RECOMMENDED case");
        check(acc.cls == ACCEPTABLE,  "classification: ACCEPTABLE case");
        check(rej.cls == REJECTED,    "classification: REJECTED case");
    }

    // 8. Invalid input is rejected, valid input accepted
    {
        std::string e;
        check(!validate_scenario(make(0, 0,      0.1, 0.2, 5, 0.04), e), "reject P = 0");
        check(!validate_scenario(make(0, -5,     0.1, 0.2, 5, 0.04), e), "reject P < 0");
        check(!validate_scenario(make(0, 1000,   0.1, -0.1, 5, 0.04), e), "reject sigma < 0");
        check(!validate_scenario(make(0, 1000,   0.1, 0.2, 0, 0.04), e), "reject T = 0");
        check(!validate_scenario(make(0, 1000,  -1.0, 0.2, 5, 0.04), e), "reject r = -100%");
        check(!validate_scenario(make(0, 1000,   0.1, 0.2, 5, -1.0), e), "reject inflation = -100%");
        check(validate_scenario(make(0, 1000,    0.1, 0.2, 5, 0.04), e), "accept a valid scenario");
    }

    // 9. Smallest case: M = 1 must not crash (stdev undefined -> score 0)
    {
        Result r = evaluate_scenario(make(0, 100000, 0.08, 0.2, 5, 0.04), 1, seed);
        check(r.score == 0.0 && std::isfinite(r.mean_real_final), "M = 1 handled safely");
    }

    std::printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}