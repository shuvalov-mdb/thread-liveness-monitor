#include <ctime>
#include <chrono>
#include <string>

#include "thread_monitor/thread_monitor_impl.h"

namespace thread_monitor {

struct ThreadMonitorStdTimeSupport {
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
};

template <unsigned int HistoryDepth = 10,
          typename TimeSupport = ThreadMonitorStdTimeSupport>
class ThreadMonitor {
public:
    ThreadMonitor(std::string name);

private:
    struct History {
        uint32_t checkpointId;
        typename TimeSupport::TimePoint timestamp;

#       ifndef NDEBUG
        // Sequence number is very expensive to generate and thus
        // it should be used only with debug builds.
        uint64_t sequence;
#       endif
    };

    const std::string _name;

    History _history[HistoryDepth];
};

void threadMonitorCheckpoint(uint32_t checkpointId);

template <unsigned int HistoryDepth,
          typename TimeSupport>
ThreadMonitor<HistoryDepth, TimeSupport>::ThreadMonitor(std::string name) : _name(name) {

}

}  // namespace thread_monitor
