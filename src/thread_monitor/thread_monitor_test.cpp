#include "thread_monitor/thread_monitor.h"

#include <thread>

#include "gtest/gtest.h"

namespace thread_monitor {
namespace {

using namespace std::chrono_literals;

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

TEST(ThreadMonitor, CheckpointWithoutMonitorIsNoOP) {
    threadMonitorCheckpoint(1);
}

TEST(ThreadMonitor, KeepsNCheckpoints) {
    for (unsigned int test = 1; test <= 20; ++test) {
        const auto testStart = std::chrono::system_clock::now();
        std::this_thread::sleep_for(1ms);
        ThreadMonitor<10> monitor("test", 0);
        std::this_thread::sleep_for(1ms);
        // Account for extra checkpoint added in constructor.
        const int expectedHistorySize = std::min<unsigned int>(monitor.depth(), test + 1);

        for (int i = 0; i < test; ++i) {
            threadMonitorCheckpoint(i + 1);
            std::this_thread::sleep_for(1ms);
        }
        const auto testStop = std::chrono::system_clock::now();

        auto history = monitor.getHistory();
        ASSERT_EQ(expectedHistorySize, history.size());
        auto lastTimestamp = testStart;
        for (int h = 0, prevId = -1; h < history.size(); ++h) {
            // Tests that ids are monotonically increasing.
            if (test < monitor.depth()) {
                ASSERT_EQ(h, history[h].checkpointId);
            }
            if (prevId != -1) {
                ASSERT_EQ(prevId + 1, history[h].checkpointId);
            }
            prevId = history[h].checkpointId;

            // Tests that timestamps are advancing and are between
            // testStart and testStop.
            ASSERT_LE(lastTimestamp, history[h].timestamp);
            ASSERT_LE(history[h].timestamp, testStop);
            lastTimestamp = history[h].timestamp;
        }
    }
}

// When checkpoints are very close in time (tight loop) the
// last one just overrides the previous.
TEST(ThreadMonitor, MergeCheckpoints) {
    while (true) {
        ThreadMonitor<10> monitor("test", 0);
        threadMonitorCheckpoint(1);
        auto history = monitor.getHistory();
        ASSERT_GE(history.size(), 1);
        if (history.size() == 1) {
            ASSERT_EQ(1, history[0].checkpointId);
            break;
        }
        // Repeat to avoid flakiness.
    }
}

}  // namespace
}  // namespace thread_monitor
