#include <chrono>

#include <benchmark/benchmark.h>

#include "thread_monitor/thread_monitor.h"

namespace thread_monitor {
namespace {

static void BM_ConcurrentCreateDelete(benchmark::State& state) {
  ThreadMonitorCentralRepository::instance();
  for (auto _ : state) {
      ThreadMonitor<> monitor("test", 1);
  }
}

BENCHMARK(BM_ConcurrentCreateDelete)->Threads(1)->MinTime(1);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(8)->MinTime(1);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(16)->MinTime(1);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(32)->MinTime(1);
BENCHMARK(BM_ConcurrentCreateDelete)->Threads(64)->MinTime(1);


static void BM_Checkpoint(benchmark::State& state) {
  ThreadMonitor<> monitor("test", 1);
  for (auto _ : state) {
      threadMonitorCheckpoint(2);
  }
}

BENCHMARK(BM_Checkpoint)->Threads(1)->MinTime(1);
BENCHMARK(BM_Checkpoint)->Threads(2)->MinTime(1);
BENCHMARK(BM_Checkpoint)->Threads(8)->MinTime(1);
BENCHMARK(BM_Checkpoint)->Threads(16)->MinTime(1);
BENCHMARK(BM_Checkpoint)->Threads(32)->MinTime(1);
BENCHMARK(BM_Checkpoint)->Threads(64)->MinTime(1);


} // namespace
} // namespace thread_monitor

BENCHMARK_MAIN();
