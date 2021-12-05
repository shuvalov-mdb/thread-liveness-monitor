#include "thread_monitor/thread_monitor.h"

#include "gtest/gtest.h"

namespace thread_monitor {

TEST(ThreadMonitor, SimpleCheckpoint) {
    ThreadMonitor<> monitor("test");
}

}  // namespace thread_monitor
