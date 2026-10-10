#include "../src/fluid_sim.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/resource.h>

namespace {
using Clock = std::chrono::steady_clock;
constexpr float step = 1.0f / 120.0f;

bool valid(const FluidSimulation& fluid) {
    for (const auto& point : fluid.vertices())
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < FluidSimulation::radius || point.x > 1440 - FluidSimulation::radius ||
            point.y < FluidSimulation::radius || point.y > 900 - FluidSimulation::radius) return false;
    return true;
}

bool measure(const FluidSimulation& snapshot, const std::vector<WindowRect>& windows,
             const std::vector<unsigned>& workers, int samples) {
    FluidSimulation settled = snapshot;
    for (int tick = 0; tick < 120; ++tick) settled.step(step, windows, false, {720, 60});
    std::vector<std::vector<double>> timings(workers.size());
    for (auto& values : timings) values.reserve(samples * 3);
    for (int repeat = 0; repeat < 3; ++repeat) {
        std::vector<FluidVertex> reference;
        // Alternate mode order across repeats to reduce temporal bias.
        for (std::size_t order = 0; order < workers.size(); ++order) {
            const std::size_t mode = repeat % 2 ? workers.size() - order - 1 : order;
            FluidSimulation fluid = settled;
            fluid.set_workers(workers[mode]);
            for (int warmup = 0; warmup < 12; ++warmup) fluid.step(step, windows, false, {720, 60});
            for (int tick = 0; tick < samples; ++tick) {
                const auto start = Clock::now();
                fluid.step(step, windows, false, {720, 60});
                timings[mode].push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
            }
            if (fluid.size() != snapshot.size() || !valid(fluid)) return false;
            if (reference.empty()) reference = fluid.vertices();
            else if (std::memcmp(reference.data(), fluid.vertices().data(), reference.size() * sizeof(FluidVertex)) != 0)
                return false;
        }
    }
    for (std::size_t mode = 0; mode < workers.size(); ++mode) {
        auto& values = timings[mode];
        double mean = 0;
        for (double value : values) mean += value;
        mean /= values.size();
        std::sort(values.begin(), values.end());
        std::printf("%zu,%zu,%u,%u,%zu,%.4f,%.4f,%.4f,%.4f,%.2f\n", snapshot.size(), windows.size(),
                    workers[mode], static_cast<unsigned>(workers[mode] > 1 && snapshot.size() >= FluidSimulation::parallel_threshold),
                    values.size(), mean, values[values.size() / 2], values[(values.size() - 1) * 95 / 100],
                    values.back(), mean * 120 / 10);
    }
    std::fflush(stdout);
    return true;
}
}

int main(int argc, char** argv) {
    bool quick = false, check_only = false, profile = false, selected = false;
    unsigned requested = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--quick") == 0) quick = true;
        else if (std::strcmp(argv[i], "--check-only") == 0) check_only = true;
        else if (std::strcmp(argv[i], "--profile") == 0) profile = true;
        else if (std::strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            const char* value = argv[++i];
            const auto result = std::from_chars(value, value + std::strlen(value), requested);
            if (result.ec != std::errc{} || *result.ptr != '\0') return 1;
            selected = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::puts("Usage: benchmark-parallel-physics.sh [--quick] [--workers N] [--check-only] [--profile]\n"
                      "Compares CPU worker limits from identical snapshots, checking bit-identical final particles.\n"
                      "Default limits: 1, 2, 4, automatic, and all logical CPU cores. --workers selects one limit plus serial.\n"
                      "--quick tests up to 2,000 particles with fewer samples. --check-only runs physics/equivalence checks.\n"
                      "--profile adds a 15-second workload using the selected or automatic worker limit.\n"
                      "Timing excludes seeding, snapshots, warmup, rendering, and window enumeration.");
            return 0;
        } else {
            std::fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return 1;
        }
    }
    if (check_only) return run_physics_checks() && run_parallel_physics_checks() ? 0 : 1;
    FluidSimulation seeded(1440, 900, requested);
    std::vector<unsigned> workers;
    const std::vector<unsigned> choices = selected ? std::vector<unsigned>{1, requested} :
                                                   std::vector<unsigned>{1, 2, 4, 0, 999};
    for (const auto choice : choices) {
        const unsigned count = FluidSimulation(1440, 900, choice).workers();
        if (std::find(workers.begin(), workers.end(), count) == workers.end()) workers.push_back(count);
    }
    std::fprintf(stderr, "Parallel physics benchmark PID: %d; optimization: O2; automatic worker limit: %u\n",
                 getpid(), FluidSimulation(1440, 900).workers());
    std::puts("particles,windows,worker_limit,parallel_active,samples,mean_ms,median_ms,p95_ms,max_ms,step_budget_percent_at_120hz");
    const std::vector<WindowRect> one_window{{500, 220, 440, 280}};
    std::vector<WindowRect> many_windows;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
            many_windows.push_back({100.0f + column * 310, 100.0f + row * 200, 210, 125});
    for (const std::size_t count : {256u, 512u, 1024u, 2000u, 4000u, 8000u}) {
        if (quick && count > 2000) break;
        std::fprintf(stderr, "Seeding %zu particles...\n", count);
        while (seeded.size() < count) seeded.step(step, one_window, true, {720, 60});
        if (!measure(seeded, one_window, workers, quick ? 60 : 180) ||
            (!quick && !measure(seeded, many_windows, workers, 180))) {
            std::fputs("FAIL: worker limits produced different or invalid particles.\n", stderr);
            return 1;
        }
    }
    if (profile) {
        std::fprintf(stderr, "Profiling %zu particles / 12 windows for 15 seconds with %u worker limit (PID %d)...\n",
                     seeded.size(), seeded.workers(), getpid());
        rusage before{}, after{};
        getrusage(RUSAGE_SELF, &before);
        const auto started = Clock::now();
        const auto deadline = started + std::chrono::seconds(15);
        while (Clock::now() < deadline) seeded.step(step, many_windows, false, {720, 60});
        getrusage(RUSAGE_SELF, &after);
        auto cpu_seconds = [](const rusage& usage) {
            return usage.ru_utime.tv_sec + usage.ru_stime.tv_sec +
                   (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 0.000001;
        };
        const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();
        std::fprintf(stderr, "Profile CPU usage: %.1f%% (100%% is one CPU core); elapsed %.2fs.\n",
                     (cpu_seconds(after) - cpu_seconds(before)) / elapsed * 100, elapsed);
        if (!valid(seeded)) return 1;
    }
    return 0;
}
