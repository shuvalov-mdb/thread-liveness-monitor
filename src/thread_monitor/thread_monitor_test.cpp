#include "thread_monitor/thread_monitor.h"

#include "gtest/gtest.h"

namespace thread_monitor {

TEST(ThreadMonitor, CantBeEnabledTwiceNested) {
  ThreadMonitor<> monitor("test", 1);
  ASSERT_TRUE(monitor.isEnabled());
  ThreadMonitor<> monitor2("test2", 2);
  ASSERT_FALSE(monitor2.isEnabled());

  ASSERT_EQ("test", monitor.name());
  ASSERT_EQ("test2", monitor2.name());
}

TEST(ThreadMonitor, CanBeEnabledTwiceNotNested) {
  {
    ThreadMonitor<> monitor("test", 1);
    ASSERT_TRUE(monitor.isEnabled());
  }
  ThreadMonitor<> monitor2("test", 2);
  ASSERT_TRUE(monitor2.isEnabled());
}

TEST(ThreadMonitor, FirstCheckpointInConstructor) {
  ThreadMonitor<> monitor("test", 1);
  auto history = monitor.getHistory();
  ASSERT_EQ(1, history.size());
  ASSERT_EQ(1, history[0].checkpointId);
}

} // namespace thread_monitor
