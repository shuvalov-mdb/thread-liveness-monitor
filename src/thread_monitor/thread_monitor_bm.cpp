#include <chrono>

#include <benchmark/benchmark.h>

#include "thread_monitor/thread_monitor.h"

namespace thread_monitor {
namespace {

static void BM_ConcurrentCreateDelete(benchmark::State& state) {
  for (auto _ : state) {
      ThreadMonitor<> monitor("test", 1);
  }
}

BENCHMARK(BM_ConcurrentCreateDelete)->Threads(1);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(8);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(16);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(32);


static void BM_Checkpoint(benchmark::State& state) {
  ThreadMonitor<> monitor("test", 1);
  for (auto _ : state) {
      threadMonitorCheckpoint(2);
  }
}

BENCHMARK(BM_Checkpoint)->Threads(1);
BENCHMARK(BM_Checkpoint)->Threads(8);
BENCHMARK(BM_Checkpoint)->Threads(16);
BENCHMARK(BM_Checkpoint)->Threads(32);


} // namespace
} // namespace thread_monitor

BENCHMARK_MAIN();
