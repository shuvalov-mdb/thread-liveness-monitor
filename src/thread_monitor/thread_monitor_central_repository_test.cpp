#include "thread_monitor/thread_monitor_central_repository.h"

#include <thread>

#include "gtest/gtest.h"
#include "thread_monitor/thread_monitor.h"

namespace thread_monitor {
namespace {

static const bool dummy = ThreadMonitorCentralRepository::instantiateWithoutMonitorThreadForTests();

TEST(ThreadMonitor, MemoryOverhead) {
    using Collection = plf::colony<ThreadMonitorCentralRepository::ThreadRegistration>;
    ASSERT_LE(sizeof(Collection),
              100);  // No more than 100 bytes per shard for empty collection.
}

TEST(CentralRepository, RegisterThread) {
    ThreadMonitor<> monitor("test", 1);
    ASSERT_EQ(1, ThreadMonitorCentralRepository::instance()->threadCount());
}

// Tests that a monitor is removed by garbage collector during monitor cycle.
TEST(CentralRepository, GarbageCollection) {
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();  // Cleanup.
    { ThreadMonitor<> monitor("test", 1); }
    ASSERT_EQ(1, ThreadMonitorCentralRepository::instance()->threadCount());
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    ASSERT_EQ(0, ThreadMonitorCentralRepository::instance()->threadCount());
}

TEST(CentralRepository, ThreadTimeout) {
    const auto frozenCount =
        ThreadMonitorCentralRepository::instance()->getLivenessErrorConditionDetectedCount();
    ThreadMonitorCentralRepository::instance()->setThreadTimeout(std::chrono::milliseconds{1});
    std::atomic<bool> livenessConditionDetected = false;
    ThreadMonitorCentralRepository::instance()->setLivenessErrorConditionDetectedCallback(
        [&] { livenessConditionDetected = true; });

    ThreadMonitor<> monitor("test", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    while (!livenessConditionDetected) {
        threadMonitorCheckpoint(2);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    }
    ASSERT_EQ(frozenCount + 1,
              ThreadMonitorCentralRepository::instance()->getLivenessErrorConditionDetectedCount());
}

// Tests that an instrumented thread updates its liveness timestamp
// in the central repository.
TEST(CentralRepository, CentralRepositoryUpdates) {
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    ThreadMonitorCentralRepository::instance()->setThreadTimeout(std::chrono::milliseconds{1000});
    ThreadMonitorCentralRepository::instance()->setReportingInterval(std::chrono::milliseconds{1});

    ThreadMonitor<> monitor("test", 1);
    auto initialStates = ThreadMonitorCentralRepository::instance()->getAllThreadLivenessStates();
    ASSERT_EQ(1, initialStates.size());
    while (true) {
        threadMonitorCheckpoint(2);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
        auto states = ThreadMonitorCentralRepository::instance()->getAllThreadLivenessStates();
        ASSERT_EQ(1, initialStates.size());
        if (initialStates[0].lastSeenAliveTimestamp < states[0].lastSeenAliveTimestamp) {
            break;  // Timestamp is updated.
        }
    }
}

}  // namespace
}  // namespace thread_monitor
