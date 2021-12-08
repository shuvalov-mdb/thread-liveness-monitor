#include "thread_monitor/thread_monitor_central_repository.h"

#include "thread_monitor/thread_monitor.h"

#include <iostream>

namespace thread_monitor {

ThreadMonitorCentralRepository* ThreadMonitorCentralRepository::_staticInstance(bool withMonitorThread) {
    static ThreadMonitorCentralRepository* inst = new ThreadMonitorCentralRepository(withMonitorThread);
    return inst;
}

ThreadMonitorCentralRepository::ThreadMonitorCentralRepository(bool withMonitorThread) {

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

ThreadMonitorCentralRepository::ThreadRegistration* ThreadMonitorCentralRepository::registerThread(
        std::thread::id threadId, details::ThreadMonitorBase* monitor,
        std::chrono::system_clock::time_point now) {
    // Keep trying shards to find a non-locked one.
    for (int shard = std::hash<std::thread::id>{}(threadId) % kShards;;) {
        // Keep trying to lock a shard.
        if (!_registrations[shard].second.try_lock()) {
            if (++shard >= kShards) { shard = 0; }
            continue;
        }
        std::lock_guard<std::mutex> lock(_registrations[shard].second, std::adopt_lock);

        plf::colony<ThreadRegistration>& coll = _registrations[shard].first;
        ThreadRegistration& r = *coll.emplace(threadId, monitor, now);
        // The plf::colony is pointer-stable, thus we can use the pointer until deleted.
        return &r;
    }
}

uint32_t ThreadMonitorCentralRepository::threadCount() const {
    uint32_t size = 0;
    for (int shard = 0; shard < kShards; ++shard) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(_registrations[shard].second));
        size += _registrations[shard].first.size();
    }
    return size;
}

void ThreadMonitorCentralRepository::runMonitorCycle() {
    const auto methodStart = std::chrono::system_clock::now();
    ThreadRegistration* frozenThread = nullptr;
    details::ThreadMonitorBase::History frozenThreadHistory;

    for (int shard = 0; shard < kShards; ++shard) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(_registrations[shard].second));
        for (auto it = _registrations[shard].first.begin(); it != _registrations[shard].first.end();) {
            // If the item is deleted garbage collect it.
            const auto lastSeenAlive = it->lastSeenAlive.load();
            if (lastSeenAlive == std::chrono::system_clock::time_point::max()) {
                {
                    // Need to lock the deletion mutex to avoid deleting the element while
                    // its destructor is holding the mutex.
                    std::lock_guard<std::mutex> elementLock(it->monitorDeletionMutex);
                    assert(it->monitor == nullptr);
                }
                it = _registrations[shard].first.erase(it);
                continue;
            }

            // The 'methodStart' is slightly stale but it's not important.
            if (methodStart - lastSeenAlive > _threadTimeout.load()) {
                // Check the actual thread structure to be sure.
                {
                    std::lock_guard<std::mutex> elementLock(it->monitorDeletionMutex);
                    if (it->monitor) {
                        const auto lastSeen = it->monitor->lastCheckpointTime();
                        if (std::chrono::system_clock::now() - lastSeen > _threadTimeout.load()) {
                            frozenThread = &(*it);
                            frozenThreadHistory = it->monitor->getHistory();
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
        std::cerr << "Frozen thread:" << std::endl;
        details::ThreadMonitorBase::printHistory(frozenThreadHistory);
    }
}

} // namespace thread_monitor
