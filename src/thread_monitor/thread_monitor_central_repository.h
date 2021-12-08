// Author: Andrew Shuvalov

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "third_party/plf_colony/plf_colony.h"

namespace thread_monitor {

namespace details {
class ThreadMonitorBase;
}  // namespace details

class ThreadMonitorCentralRepository {
public:
    static inline constexpr auto kDefaultThreadTimeout = std::chrono::minutes{5};

    struct ThreadRegistration {
        std::thread::id threadId;
        // The timestamp updated by the thread itself to indicate
        // it is alive. For efficiency, this timestamp is not updated
        // on every checkpoint, but only every few seconds.
        std::atomic<std::chrono::system_clock::time_point> lastSeenAlive;
        // Guards 'monitor' pointer protecting from deletion.
        // This mutex is not used by checkpoints, thus the lock
        // contention should be extremely low.
        std::mutex monitorDeletionMutex;
        // In destructor, the monitor clears this pointer.
        details::ThreadMonitorBase* monitor;

        ThreadRegistration(std::thread::id threadId, details::ThreadMonitorBase* monitor,
            std::chrono::system_clock::time_point now)
            : threadId(threadId), lastSeenAlive(now), monitor(monitor) {}
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
     * Changes how often the new thread monitors will need to update the liveness timestamp.
     * In production, keep the default value, no need to change this.
     * In integration and stress tests, reduce the interval to spot the lagging threads
     * more accurately.
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
     * Register this thread monitor with central repository. This has to be done from the
     * monitor constructor.
     * There is no de-registration method. Instead, the monitor clears the pointer to
     * itself and the repository garbage collects the removed monitors later.
     */
    ThreadRegistration* registerThread(std::thread::id threadId, details::ThreadMonitorBase* monitor,
        std::chrono::system_clock::time_point now);

    /**
     * Approximate (stale) count of registered threads.
     * The count should sum several shards, each shard is locked separately.
     */
    uint32_t threadCount() const;

    /**
     * Internal method to start a monitor cycle. Can be invoked directly in tests.
     * In production, this is invoked from the monitor thread.
     */
    void runMonitorCycle();

protected:
    ThreadMonitorCentralRepository(bool withMonitorThread = true);
    ~ThreadMonitorCentralRepository() = default;

private:
    // Lock contention hits harder with lower count.
    static inline constexpr int kShards = 40;

    // Returns 'was deleted'. If deleted, changes iterator in place.
    // Precondition: must be invoked under lock.
    bool _maybeGarbageCollectRecord(
        plf::colony<ThreadRegistration>& collection, plf::colony<ThreadRegistration>::iterator& it);

    static ThreadMonitorCentralRepository* _staticInstance(bool withMonitorThread);

    std::atomic<std::chrono::system_clock::duration> _threadTimeout =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(kDefaultThreadTimeout);

    std::chrono::system_clock::time_point _lastTimeOfFaultAction = std::chrono::system_clock::now();

    using LockableColony = std::pair<plf::colony<ThreadRegistration>, std::mutex>;
    // Keeps all thread registrations in the pointer-stable, continuous collection.
    // A non-locked shard is picked at registration time.
    // Each shard has its own mutex.
    std::array<LockableColony, kShards> _registrations;
};

} // namespace thread_monitor
