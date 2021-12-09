#include "thread_monitor/thread_monitor_central_repository.h"

#include "thread_monitor/thread_monitor.h"

#include <iostream>

namespace thread_monitor {

ThreadMonitorCentralRepository* ThreadMonitorCentralRepository::_staticInstance(
    bool withMonitorThread) {
    static ThreadMonitorCentralRepository* inst =
        new ThreadMonitorCentralRepository(withMonitorThread);
    return inst;
}

ThreadMonitorCentralRepository::ThreadMonitorCentralRepository(bool withMonitorThread) {
    if (withMonitorThread) {
        auto* t = new std::thread([this] {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            while (!_terminating) {
                // This does both GC and frozen thread detection.
                // In steady production load with up to 1k threads this cycle takes
                // about 1 microsec, so not much over head to run every few millis.
                const auto garbageCollected = runMonitorCycle();
                // Decides how long to sleep depending on GC count.
                if (garbageCollected > 500) {
                    // Heavy GC, repeat soon.
                    std::this_thread::sleep_for(std::chrono::microseconds{200});
                    continue;
                }
                if (garbageCollected > 100) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{5});
                    continue;
                }
                if (garbageCollected > 10) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});
                    continue;
                }
                std::this_thread::sleep_for(kIdleMonitorCycleInterval);
            }
        });
        _monitorThread = std::unique_ptr<std::thread>(t);
    }
}

ThreadMonitorCentralRepository::~ThreadMonitorCentralRepository() {
    _terminating = true;
    if (_monitorThread) {
        _monitorThread->join();
    }
}

ThreadMonitorCentralRepository* ThreadMonitorCentralRepository::instance() {
    return _staticInstance(true);
}

bool ThreadMonitorCentralRepository::instantiateWithoutMonitorThreadForTests() {
    _staticInstance(false);
    return true;
}

void ThreadMonitorCentralRepository::setThreadTimeout(std::chrono::system_clock::duration timeout) {
    _threadTimeout = timeout;
}

std::chrono::system_clock::duration ThreadMonitorCentralRepository::reportingInterval() const {
    return _reportingInterval;
}

void ThreadMonitorCentralRepository::setReportingInterval(
    std::chrono::system_clock::duration interval) {
    _reportingInterval = interval;
}

void ThreadMonitorCentralRepository::setLivenessErrorConditionDetectedCallback(
    std::function<void()> cb) {
    _frozenConditionCallback = cb;
}

ThreadMonitorCentralRepository::ThreadRegistration* ThreadMonitorCentralRepository::registerThread(
    std::thread::id threadId,
    details::ThreadMonitorBase* monitor,
    std::chrono::system_clock::time_point now) {
    const int shard = std::hash<std::thread::id>{}(threadId) % kShards;
    std::lock_guard<std::mutex> lock(std::get<2>(_registrations[shard]));

    plf::colony<ThreadRegistration>& coll = std::get<0>(_registrations[shard]);
    auto it = coll.emplace(threadId, monitor, now);
    ThreadRegistration& r = *it;
    return &r;
}

uint32_t ThreadMonitorCentralRepository::threadCount() const {
    uint32_t size = 0;
    for (int shard = 0; shard < kShards; ++shard) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(std::get<2>(_registrations[shard])));
        size += std::get<0>(_registrations[shard]).size();
    }
    return size;
}

std::vector<ThreadMonitorCentralRepository::ThreadLivenessState>
ThreadMonitorCentralRepository::getAllThreadLivenessStates() const {
    std::vector<ThreadLivenessState> states;
    for (int shard = 0; shard < kShards; ++shard) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(std::get<2>(_registrations[shard])));
        for (const auto& r : std::get<0>(_registrations[shard])) {
            ThreadLivenessState state;
            state.lastSeenAliveTimestamp = r.lastSeenAlive.load();
            state.threadId = r.threadId;
            states.emplace_back(std::move(state));
        }
    }
    return states;
}

uint32_t ThreadMonitorCentralRepository::getLivenessErrorConditionDetectedCount() const {
    return _frozenConditionsDetected;
}

unsigned int ThreadMonitorCentralRepository::runMonitorCycle() {
    const auto methodStart = std::chrono::system_clock::now();
    const auto oldestAliveTimestampThreshold = methodStart - _threadTimeout.load();
    ThreadRegistration* frozenThread = nullptr;
    details::ThreadMonitorBase::History frozenThreadHistory;
    std::thread::id frozenThreadId;
    unsigned int garbageCollected = 0;

    for (int shard = 0; shard < kShards; ++shard) {
        LockableColony& registration = _registrations[shard];
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(std::get<2>(registration)));
        for (auto it = std::get<0>(registration).begin();
             it != std::get<0>(registration).end();) {
            // If the item is deleted garbage collect it.
            const bool wasErased = _maybeGarbageCollectRecord(std::get<0>(registration), it);
            if (wasErased) {
                ++garbageCollected;
                continue;  // Do not ++it, it was updated by GC.
            }

            // The 'methodStart' is slightly stale but it's not important.
            if (it->lastSeenAlive.load() < oldestAliveTimestampThreshold) {
                // Check the actual thread structure to be sure.
                {
                    // Any access to it->monitor must be guarded.
                    std::lock_guard<std::mutex> elementLock(it->monitorDeletionMutex);
                    if (it->monitor) {
                        const auto lastSeen = it->monitor->lastCheckpointTime();
                        if (std::chrono::system_clock::now() - lastSeen > _threadTimeout.load()) {
                            frozenThread = &(*it);
                            frozenThreadHistory = it->monitor->getHistory();
                            frozenThreadId = it->threadId;
                            break;
                        }
                    }
                }
            }

            ++it;
        }
        if (frozenThread != nullptr) {
            break;
        }
    }

    if (frozenThread != nullptr && methodStart - _lastTimeOfFaultAction > _threadTimeout.load()) {
        _lastTimeOfFaultAction = methodStart;
        _frozenConditionsDetected.fetch_add(1);
        std::cerr << "Frozen thread: " << frozenThreadId << std::endl;
        details::ThreadMonitorBase::printHistory(frozenThreadHistory);
        _frozenThreadAction();
    }
    return garbageCollected;
}

void ThreadMonitorCentralRepository::_frozenThreadAction() {
    // Print all threads that are stale for more than configured value to
    // avoid unnecessary verbosity.
    std::cerr << "All stale threads:" << std::endl;
    for (int shard = 0; shard < kShards; ++shard) {
        const auto shardStart = std::chrono::system_clock::now();
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(std::get<2>(_registrations[shard])));
        for (auto it = std::get<0>(_registrations[shard]).begin(); it != std::get<0>(_registrations[shard]).end();
             ++it) {
            auto lastSeenAlive = it->lastSeenAlive.load();
            if (lastSeenAlive == std::chrono::system_clock::time_point::max() ||
                shardStart - lastSeenAlive < kStaleThreadThreshold) {
                continue;
            }
            // Need to obtain more fresh history under lock.
            details::ThreadMonitorBase::History threadHistory;
            std::thread::id threadId;
            {
                // Any access to it->monitor must be guarded.
                std::lock_guard<std::mutex> elementLock(it->monitorDeletionMutex);
                if (it->monitor) {
                    threadHistory = it->monitor->getHistory();
                    threadId = it->threadId;
                }
            }
            lastSeenAlive = threadHistory[threadHistory.size() - 1].timestamp;
            // A race is possible that the thread was unregistered.
            if (lastSeenAlive == std::chrono::system_clock::time_point::max() ||
                threadHistory.empty() || shardStart - lastSeenAlive < kStaleThreadThreshold) {
                continue;
            }
            std::cerr << "Thread: " << threadId << std::endl;
            details::ThreadMonitorBase::printHistory(threadHistory);
        }
    }

    if (_frozenConditionCallback) {
        _frozenConditionCallback();
    }
}

inline bool ThreadMonitorCentralRepository::_maybeGarbageCollectRecord(
    plf::colony<ThreadRegistration>& collection, plf::colony<ThreadRegistration>::iterator& it) {

    if (it->lastSeenAlive.load() == std::chrono::system_clock::time_point::max()) {
        {
            // Need to lock the deletion mutex to avoid deleting the element while
            // its destructor is holding the mutex.
            std::lock_guard<std::mutex> elementLock(it->monitorDeletionMutex);
            assert(it->monitor == nullptr);
        }
        it = collection.erase(it);
        return true;
    }
    return false;
}

}  // namespace thread_monitor
