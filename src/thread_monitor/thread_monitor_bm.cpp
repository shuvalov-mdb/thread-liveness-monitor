#include <chrono>

#include <benchmark/benchmark.h>

#include "thread_monitor/thread_monitor.h"

namespace thread_monitor {
namespace {

static void BM_ConcurrentCreateDelete(benchmark::State& state) {
    if (state.thread_index == 0) {
        ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    }
    for (auto _ : state) {
        ThreadMonitor<> monitor("test", 1);
    }
}

BENCHMARK(BM_ConcurrentCreateDelete)->Threads(1)->MinTime(1)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(1)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(4)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(8)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(16)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(32)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(64)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(128)->MinTime(5)->UseRealTime();
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(1024)->MinTime(5)->UseRealTime();

static void BM_Checkpoint(benchmark::State& state) {
    if (state.thread_index == 0) {
        ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    }
    ThreadMonitor<> monitor("test", 1);
    for (auto _ : state) {
        threadMonitorCheckpoint(2);
    }
}

BENCHMARK(BM_Checkpoint)->Threads(1)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(4)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(8)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(16)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(32)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(64)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(128)->MinTime(1)->UseRealTime();
BENCHMARK(BM_Checkpoint)->Threads(1024)->MinTime(1)->UseRealTime();

static void BM_FullCycle(benchmark::State& state) {
    if (state.thread_index == 0) {
        ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    }
    for (auto _ : state) {
        ThreadMonitor<> monitor("test", 1);
        for (int i = 0; i < state.range(0); ++i) {
            threadMonitorCheckpoint(2);
        }
    }
}

BENCHMARK(BM_FullCycle)->Args({100})->Threads(1)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(4)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(8)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(16)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(32)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(64)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(128)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({100})->Threads(1024)->MinTime(5)->UseRealTime();

BENCHMARK(BM_FullCycle)->Args({1000})->Threads(1)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(4)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(8)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(16)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(32)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(64)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(128)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({1000})->Threads(1024)->MinTime(5)->UseRealTime();

BENCHMARK(BM_FullCycle)->Args({10000})->Threads(1)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(4)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(8)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(16)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(32)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(64)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(128)->MinTime(5)->UseRealTime();
BENCHMARK(BM_FullCycle)->Args({10000})->Threads(1024)->MinTime(5)->UseRealTime();

static void BM_GCAndMonitor(benchmark::State& state) {
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    ThreadMonitor<> monitor("test", 1);
    for (auto _ : state) {
        ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    }
}

BENCHMARK(BM_GCAndMonitor)->Threads(1)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(8)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(32)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(64)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(128)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(256)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(512)->MinTime(1)->UseRealTime();
BENCHMARK(BM_GCAndMonitor)->Threads(1024)->MinTime(1)->UseRealTime();

}  // namespace
}  // namespace thread_monitor

BENCHMARK_MAIN();
