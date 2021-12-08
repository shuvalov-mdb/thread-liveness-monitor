#include "thread_monitor/thread_monitor_central_repository.h"

#include <thread>

#include "gtest/gtest.h"
#include "thread_monitor/thread_monitor.h"

namespace thread_monitor {
namespace {

static const bool dummy = ThreadMonitorCentralRepository::instantiateWithoutMonitorThreadForTests();

TEST(CentralRepository, RegisterThread) {
    ThreadMonitor<> monitor("test", 1);
    ASSERT_EQ(1, ThreadMonitorCentralRepository::instance()->threadCount());
}

// Tests that a monitor is removed by garbage collector during monitor cycle.
TEST(CentralRepository, GarbageCollection) {
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();  // Cleanup.
    {
        ThreadMonitor<> monitor("test", 1);
    }
    ASSERT_EQ(1, ThreadMonitorCentralRepository::instance()->threadCount());
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();
    ASSERT_EQ(0, ThreadMonitorCentralRepository::instance()->threadCount());
}

TEST(CentralRepository, ThreadTimeout) {
    ThreadMonitorCentralRepository::instance()->setThreadTimeout(std::chrono::milliseconds{1});
    ThreadMonitor<> monitor("test", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    ThreadMonitorCentralRepository::instance()->runMonitorCycle();

}

} // namespace
} // namespace thread_monitor
