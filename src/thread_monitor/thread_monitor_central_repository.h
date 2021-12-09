// Author: Andrew Shuvalov

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "third_party/plf_colony/plf_colony.h"

namespace thread_monitor {

namespace details {
class ThreadMonitorBase;
}  // namespace details

class ThreadMonitorCentralRepository {
public:
    static inline constexpr auto kDefaultThreadTimeout = std::chrono::minutes{5};
    static inline constexpr auto kStaleThreadThreshold = std::chrono::milliseconds{1};
    // How often the central repository seen alive timestamp is updated.
    // This is prorated to avoid cache misses.
    static inline constexpr auto kDefaultReportingInterval =
#ifdef NDEBUG
        std::chrono::seconds{1};  // Production.
#else
        std::chrono::milliseconds{1};  // Debug can have performance penalty.
#endif
    // The interval to spin the monitor when there is no much activity.
    // Essentially, this is idle machine overhead. With ~100 instrumented threads
    // the monitor cycle takes about 1 microsec.
    // The monitor is using adaptive intervals to spin more often when busy.
    static inline constexpr auto kIdleMonitorCycleInterval = std::chrono::milliseconds{500};

#pragma pack(push, 1)
    struct ThreadRegistration {
        // Guards 'monitor' pointer protecting from deletion.
        // This mutex is not used by checkpoints, thus the lock
        // contention should be extremely low.
        std::mutex monitorDeletionMutex;
        // The timestamp updated by the thread itself to indicate
        // it is alive. For efficiency, this timestamp is not updated
        // on every checkpoint, but only every few seconds.
        std::atomic<std::chrono::system_clock::time_point> lastSeenAlive;
        // In destructor, the monitor clears this pointer.
        details::ThreadMonitorBase* monitor;
        std::thread::id threadId;

        ThreadRegistration(std::thread::id threadId,
                           details::ThreadMonitorBase* monitor,
                           std::chrono::system_clock::time_point now)
            : threadId(threadId), lastSeenAlive(now), monitor(monitor) {}
    };
#pragma pack(pop)

    struct ThreadLivenessState {
        std::thread::id threadId;
        std::chrono::system_clock::time_point lastSeenAliveTimestamp;
    };

    /**
     * Returns the singleton.
     */
    static ThreadMonitorCentralRepository* instance();

    /**
     * Sets the internal property to not schedule the monitoring thread for tests.
     * Returns a dummy boolean to instantiate as a static in tests.
     */
    static bool instantiateWithoutMonitorThreadForTests();

    /**
     * Thread monitors do not update the central repository on every checkpoint,
     * this is too expensive. Instead, they use this interval to update.
     */
    std::chrono::system_clock::duration reportingInterval() const;

    /**
     * Changes how often the new thread monitors will need to update the liveness
     * timestamp. In production, keep the default value, no need to change this.
     * In integration and stress tests, reduce the interval to spot the lagging
     * threads more accurately.
     */
    void setReportingInterval(std::chrono::system_clock::duration interval);

    /**
     * Changes the deafault interval between monitoring cycles.
     */
    void setMonitoringInterval(std::chrono::system_clock::duration interval);

    /**
     * Sets how long the thread should be stale before it is considered not live
     * anymore (frozen, deadlocked), which triggers the fault procedures.
     */
    void setThreadTimeout(std::chrono::system_clock::duration timeout);

    /**
     * Sets callback to be invoked when a thread liveness error condition is detected.
     * In production, this callback may be set to terminate the program.
     */
    void setLivenessErrorConditionDetectedCallback(std::function<void()> cb);

    /**
     * Approximate (stale) count of registered threads.
     * The count should sum several shards, each shard is locked separately.
     */
    uint32_t threadCount() const;

    /**
     * Returns the count of how many times the frozen condition was detected.
     */
    uint32_t getLivenessErrorConditionDetectedCount() const;

    /**
     * Returns the snapshot of all instrumented threads with latest
     * liveness timestamp for every thread. Timestamps are stale to the
     * interval configured with 'reportingInterval()'.
     */
    std::vector<ThreadLivenessState> getAllThreadLivenessStates() const;

    /**
     * Internal method to register this thread monitor with central repository.
     * This has to be done from the monitor constructor. There is no de-registration
     * method. Instead, the monitor clears the pointer to itself and the repository garbage
     * collects the removed monitors later.
     */
    ThreadRegistration* registerThread(std::thread::id threadId,
                                       details::ThreadMonitorBase* monitor,
                                       std::chrono::system_clock::time_point now);

    /**
     * Internal method to start a monitor cycle. Can be invoked directly in tests.
     * In production, this is invoked from the monitor thread.
     * Returns the count of GC elements.
     */
    unsigned int runMonitorCycle();

protected:
    ThreadMonitorCentralRepository(bool withMonitorThread = true);
    ~ThreadMonitorCentralRepository();

private:
    // Lock contention hits harder with lower count.
    // In the benchmarks, 30 shards is about 30% faster than 20 shards, and
    // 40 is already in the saturation zone.
    static inline constexpr int kShards = 36;

    // Returns 'was deleted'. If deleted, changes iterator in place.
    // Precondition: must be invoked under lock.
    bool _maybeGarbageCollectRecord(plf::colony<ThreadRegistration>& collection,
                                    plf::colony<ThreadRegistration>::iterator& it);

    void _frozenThreadAction();

    static ThreadMonitorCentralRepository* _staticInstance(bool withMonitorThread);

    std::atomic<std::chrono::system_clock::duration> _threadTimeout =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(kDefaultThreadTimeout);

    std::chrono::system_clock::duration _reportingInterval = kDefaultReportingInterval;

    // This is invoked when the thread liveness failure condition is detected.
    std::function<void()> _frozenConditionCallback;

    std::chrono::system_clock::time_point _lastTimeOfFaultAction = std::chrono::system_clock::now();

    std::atomic<bool> _terminating{false};
    std::unique_ptr<std::thread> _monitorThread;

    // Separates mostly constants above from frequently changind data below.
    char __dummyCacheLinePadding[64];

    using LockableColony = std::tuple<plf::colony<ThreadRegistration>, char[16], std::mutex, char[64]>;
    // Keeps all thread registrations in the pointer-stable, continuous
    // collection. A non-locked shard is picked at registration time. Each shard
    // has its own mutex.
    std::array<LockableColony, kShards> _registrations;

    // Stats.
    std::atomic<uint32_t> _frozenConditionsDetected;
};

}  // namespace thread_monitor
