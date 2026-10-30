// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// What every performance metric measures right now, beside its budget - the
// tool for setting or moving one (docs/testing.md, "Performance tests"). These
// cases assert nothing and are hidden, so no test run selects them:
//
//   cube_perf_tests "[.perf-report]"          everything, with the corpus per seed
//   cube_perf_tests "[.perf-report-timing]"   only the wall-time metrics
//   cube_perf_tests "[.perf-report-zone-cost]" what one profiling macro costs
//                                              in this build and this state
//
// The timing half is only worth reading from an optimised build.

#include <catch2/catch_test_macros.hpp>

#include "budgets.hpp"
#include "measure.hpp"
#include "metrics.hpp"
#include "perf_build.hpp"

#include <cube/profile/session.hpp>
#include <cube/profiling.hpp>
#include <cube/solver/solver.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {

using cube::perf::BeginnerWork;
using cube::perf::MethodWork;
using cube::solver::SolveMethod;

constexpr std::string_view method_name(SolveMethod method) {
    switch (method) {
        case SolveMethod::Beginner:
            return "beginner";
        case SolveMethod::Intermediate:
            return "intermediate";
        case SolveMethod::Advanced:
            return "advanced";
    }
    return "?";
}

void print_build() {
    std::println("build: {}  optimised {}  timing meaningful {}", cube::perf::kBuildType,
                 cube::perf::kOptimised ? 1 : 0, cube::perf::kTimingMeaningful ? 1 : 0);
}

// One line per metric: what it measures, its budget, the room between them
// (how far above the measurement the budget sits), and where it came from.
void print_metrics(const char* title, const std::vector<cube::perf::Metric>& metrics) {
    std::println("\n{}", title);
    for (const cube::perf::Metric& m : metrics) {
        const double headroom =
            m.measured > 0.0 ? 100.0 * (m.budget - m.measured) / m.measured : 0.0;
        std::string where;
        if (m.seed != cube::perf::kNoSeed) {
            where = "seed " + std::to_string(m.seed);
        }
        if (!m.note.empty()) {
            where += (where.empty() ? "" : "  ") + m.note;
        }
        std::println("  {:<46} {:>14.2f} / {:<14.2f} {:<18} {:>+7.0f}%  {}{}", m.name, m.measured,
                     m.budget, m.unit, headroom, m.measured > m.budget ? "OVER  " : "", where);
    }
}

void print_timing() {
    print_metrics("timing: solves (fastest run against the calibration workload)",
                  cube::perf::solve_timing_metrics());
    print_metrics("timing: n x n solves", cube::perf::nxn_timing_metrics());
    print_metrics("timing: reads", cube::perf::read_timing_metrics());
    print_metrics("timing: scan", cube::perf::scan_timing_metrics());
}

// The beginner solve of each seed, stage by stage.
void print_beginner_per_seed() {
    std::println("\nbeginner, per seed (nodes per stage; largest search of the solve; moves)");
    std::println("{:>4} {:>9} {:>9} {:>9} {:>9} {:>9} | {:>9} {:>9} | {:>5} {:>7}", "seed", "cross",
                 "corners", "mid-edge", "oll", "pll", "solve", "largest", "moves", "ms");
    std::uint64_t worst_total = 0;
    for (unsigned seed = 0; seed < cube::perf::budget::kSolverSeeds; ++seed) {
        const auto start = cube::perf::Clock::now();
        const BeginnerWork work = cube::perf::measure_beginner(cube::perf::corpus_scramble(seed));
        const double ms = cube::perf::milliseconds_since(start);
        std::string stages;
        for (const cube::perf::StageWork& stage : work.stages) {
            stages += std::format(" {:>9}", stage.work.nodes);
        }
        std::println("{:>4}{} | {:>9} {:>9} | {:>5} {:>7.0f}{}", seed, stages, work.total.nodes,
                     work.total.largest, work.moves, ms, work.solved ? "" : "  UNSOLVED");
        worst_total = std::max(worst_total, work.total.nodes);
    }
    std::println("worst solve: {} nodes", worst_total);
}

// The other two methods, a line per seed.
void print_methods_per_seed() {
    for (const SolveMethod method : {SolveMethod::Intermediate, SolveMethod::Advanced}) {
        cube::solver::prepare(method);
        std::println("\n{}, per seed (nodes; searches; largest; moves)", method_name(method));
        for (unsigned seed = 0; seed < cube::perf::budget::kSolverSeeds; ++seed) {
            const auto start = cube::perf::Clock::now();
            const MethodWork work =
                cube::perf::measure_method(cube::perf::corpus_scramble(seed), method);
            std::println("{:>4} {:>11} {:>5} {:>11} {:>4} {:>7.1f} ms{}", seed, work.work.nodes,
                         work.work.searches, work.work.largest, work.moves,
                         cube::perf::milliseconds_since(start), work.solved ? "" : "  UNSOLVED");
        }
    }
}

}  // namespace

TEST_CASE("report: what the solver's work and the reader's buffers measure now",
          "[.perf-report][perf-report]") {
    print_build();
    print_beginner_per_seed();
    print_methods_per_seed();

    // --- every metric, beside its budget ----------------------------------
    print_metrics("beginner", cube::perf::beginner_metrics());
    print_metrics("intermediate", cube::perf::intermediate_metrics());
    print_metrics("advanced", cube::perf::advanced_metrics());
    print_metrics("n x n", cube::perf::nxn_metrics());
    print_metrics("scan", cube::perf::scan_metrics());
    print_metrics("judgement", cube::perf::judgement_metrics());
    print_timing();
}

TEST_CASE("report: what the wall-time metrics measure now", "[.perf-report-timing]") {
    print_build();
    print_timing();
}

namespace {

// Nanoseconds per iteration of `body`, the fastest of several runs of
// `iterations`: the minimum is what the instruction stream costs, the rest is
// whatever else the machine was doing.
template <typename Body>
double nanoseconds_per_iteration(const Body& body, std::uint64_t iterations) {
    double best = 0.0;
    for (int run = 0; run < 7; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t i = 0; i < iterations; ++i) {
            body(i);
        }
        const std::chrono::duration<double, std::nano> elapsed =
            std::chrono::steady_clock::now() - start;
        const double per = elapsed.count() / static_cast<double>(iterations);
        best = run == 0 ? per : std::min(best, per);
    }
    return best;
}

}  // namespace

// What one profiling macro costs in the state the build is in: compiled out
// (the macros are empty), compiled in and dormant (no CUBEULATOR_PROFILE: one
// atomic load and a branch), or started with no profiler attached
// (CUBEULATOR_PROFILE=1; Tracy's on-demand mode), and - with a capture
// attached - recording. The figure is the loop with the macro minus the same
// loop without it. docs/profiling.md, "Idle cost".
TEST_CASE("report: what one profiling macro costs", "[.perf-report-zone-cost]") {
    print_build();
    // NOLINTNEXTLINE(concurrency-mt-unsafe) - read before any other thread exists.
    const char* profile = std::getenv(cube::profile::environment_variable);
    const cube::profile::Session session(cube::profile::decide_startup(false, profile, {}));
    std::println("profiling macros: {}; started: {}",
                 cube::profiling::enabled ? "compiled in" : "compiled out",
                 session.running() ? "yes" : "no");

    constexpr std::uint64_t kIterations = 20'000'000;
    volatile std::uint64_t sink = 0;
    const double bare =
        nanoseconds_per_iteration([&sink](std::uint64_t i) { sink = sink + i; }, kIterations);
    const double zone = nanoseconds_per_iteration(
        [&sink](std::uint64_t i) {
            CUBE_ZONE_SCOPED_N("perf.zone_cost");
            sink = sink + i;
        },
        kIterations);
    const double plot = nanoseconds_per_iteration(
        [&sink](std::uint64_t i) {
            CUBE_PLOT("perf.zone_cost_plot", 1.0);
            sink = sink + i;
        },
        kIterations);
    const double frame = nanoseconds_per_iteration(
        [&sink](std::uint64_t i) {
            CUBE_FRAME_MARK_NAMED("perf.zone_cost_frames");
            sink = sink + i;
        },
        kIterations);
    std::println("{:<28} {:>8.3f} ns per iteration", "loop alone", bare);
    std::println("{:<28} {:>8.3f} ns more", "CUBE_ZONE_SCOPED_N", zone - bare);
    std::println("{:<28} {:>8.3f} ns more", "CUBE_PLOT", plot - bare);
    std::println("{:<28} {:>8.3f} ns more", "CUBE_FRAME_MARK_NAMED", frame - bare);
}
