#include <chrono>

#include <benchmark/benchmark.h>

namespace thread_monitor {
namespace {

void BM_system_clock(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(std::chrono::system_clock::now());
    }
}

void BM_steady_clock(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(std::chrono::steady_clock::now());
    }
}

void BM_high_resolution_clock(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(std::chrono::high_resolution_clock::now());
    }
}

BENCHMARK(BM_system_clock);
BENCHMARK(BM_steady_clock);
BENCHMARK(BM_high_resolution_clock);

BENCHMARK(BM_system_clock)->Threads(1);
BENCHMARK(BM_system_clock)->Threads(8);
BENCHMARK(BM_system_clock)->Threads(16);
BENCHMARK(BM_system_clock)->Threads(32);
BENCHMARK(BM_system_clock)->Threads(64);
BENCHMARK(BM_system_clock)->Threads(1024);

}  // namespace
}  // namespace thread_monitor

BENCHMARK_MAIN();
