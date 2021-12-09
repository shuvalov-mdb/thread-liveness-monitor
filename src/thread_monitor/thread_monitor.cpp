#include "thread_monitor/thread_monitor.h"

#include <cassert>
#include <iomanip>
#include <iostream>

namespace thread_monitor {

namespace details {

// If checkpoints are this close, just override the last one to avoid saving
// too many very close checkpoints.
static constexpr auto kHistoryResolution = std::chrono::microseconds{10};

#ifndef NDEBUG
std::atomic<uint64_t> ThreadMonitorBase::_globalSequence;
#endif

namespace {
thread_local ThreadMonitorBase* threadLocalPtr = nullptr;
}  // namespace

ThreadMonitorBase::ThreadMonitorBase(std::string name,
                                     InternalHistoryRecord* historyPtr,
                                     uint32_t historyDepth,
                                     uint32_t firstCheckpointId)
    : _name(std::move(name)), _historyPtr(historyPtr), _historyDepth(historyDepth) {
    _maybeRegisterThreadLocal();
    if (!_enabled) {
        return;
    }
    checkpointInternalImpl(firstCheckpointId);
    auto* const centralRepo = ThreadMonitorCentralRepository::instance();
    _registration = centralRepo->registerThread(
        _threadId, this, _creationTimestamp + _historyPtr[0].durationFromCreation.load());
    _centralRepoUpdateInterval = centralRepo->reportingInterval();
}

ThreadMonitorBase::~ThreadMonitorBase() {
    if (!_enabled) {
        return;
    }
    threadLocalPtr = nullptr;

    std::lock_guard<std::mutex> lock(_registration->monitorDeletionMutex);
    // The registration garbage collector will pick up the deleted registration.
    _registration->monitor = nullptr;
    _registration->lastSeenAlive = std::chrono::system_clock::time_point::max();
}

bool ThreadMonitorBase::isEnabled() const {
    return _enabled;
}

const std::string& ThreadMonitorBase::name() const {
    return _name;
}

unsigned int ThreadMonitorBase::depth() const {
    return _historyDepth;
}

void ThreadMonitorBase::_maybeRegisterThreadLocal() {
    if (threadLocalPtr != nullptr) {
        _enabled = false;
        return;  // Not registering, previously registered up-stack.
    }
    threadLocalPtr = this;
    _enabled = true;
}

void ThreadMonitorBase::checkpointInternalImpl(uint32_t id) {
    if (!_enabled) {
        return;
    }

    if (_headHistoryRecord == _historyDepth) {
        // Very first checkpoint (inserted from constructor).
        _historyPtr[0].checkpointId = id;
        _historyPtr[0].durationFromCreation = std::chrono::system_clock::duration::zero();
#ifndef NDEBUG
        _historyPtr[0].sequence = ++_globalSequence;
#endif
        _headHistoryRecord = 0;
        _tailHistoryRecord = 0;  // Inclusive.
        return;
    }

    const auto now = std::chrono::system_clock::now();
    if ((now - _creationTimestamp) -
            _historyPtr[_tailHistoryRecord.load()].durationFromCreation.load() <
        kHistoryResolution) {
        // We do not pollute the history with very close values. Instead, replace
        // the last one. This optimization did not affect the benchmarks.
        writeCheckpointAtPosition(_tailHistoryRecord.load(), id, now);
        maybeUpdateCentralRepository(now);
        return;
    }

    // The circular buffer write is not atomic. 1. Advance the head if needed.
    unsigned int nextIndex;
    const bool tailCaughtHead = _headHistoryRecord.load() == _tailHistoryRecord.load() + 1 ||
        (_tailHistoryRecord.load() == _historyDepth - 1 && _headHistoryRecord.load() == 0);
    if (tailCaughtHead) {
        nextIndex = _headHistoryRecord.load();  // Where head is now.
        if (++_headHistoryRecord >= _historyDepth) {
            _headHistoryRecord = 0;
        }
    } else {
        // Head remains unchanged, will write after tail.
        nextIndex = _tailHistoryRecord.load() + 1;
        if (nextIndex >= _historyDepth) {
            nextIndex = 0;
        }
    }
    // 2. Write next record without advancing the tail.
    writeCheckpointAtPosition(nextIndex, id, now);
    // 3. Advance the tail to point to the new record.
    _tailHistoryRecord = nextIndex;
    maybeUpdateCentralRepository(now);
}

void ThreadMonitorBase::writeCheckpointAtPosition(uint32_t index,
                                                  uint32_t id,
                                                  std::chrono::system_clock::time_point timestamp) {
    assert(index >= 0);
    assert(index < _historyDepth);
    InternalHistoryRecord& r = *(_historyPtr + index);
    r.checkpointId = id;
    r.durationFromCreation = timestamp - _creationTimestamp;
#ifndef NDEBUG
    r.sequence = ++_globalSequence;  // Only in debug mode, very expensive.
#endif
}

ThreadMonitorBase::History ThreadMonitorBase::getHistory() const {
    ThreadMonitorBase::History history;
    // This code is not atomic. It is only guaranteed that the thread
    // monitor is protected from deletion.
    // TODO: add external deletion mutex.
    const auto initialHead = _headHistoryRecord.load();
    bool atFirstElement = true;
    for (auto index = initialHead;;) {
        HistoryRecord h;
        InternalHistoryRecord& r = *(_historyPtr + index);
        h.checkpointId = r.checkpointId;
        h.timestamp = _creationTimestamp + r.durationFromCreation.load();
#ifndef NDEBUG
        h.sequence = r.sequence;
#endif
        // Subtle race: if head moved while we processed the 1st element
        // we should not insert it. We obviously assume that no more than
        // 1 checkpoint could be added while we are in this method, otherwise
        // it's improper use of this library.
        if (!atFirstElement || initialHead == _headHistoryRecord.load()) {
            history.push_back(std::move(h));
        }
        atFirstElement = false;
        // Tail is inclusive.
        if (index == _tailHistoryRecord.load()) {
            break;
        }
        if (++index >= _historyDepth) {
            index = 0;
        }
    }
    return history;
}

std::chrono::system_clock::time_point ThreadMonitorBase::lastCheckpointTime() const {
    while (true) {
        const auto initialTail = _tailHistoryRecord.load();
        const auto timestamp =
            _creationTimestamp + _historyPtr[initialTail].durationFromCreation.load();
        // Subtle race - is the tail still there?
        if (initialTail == _tailHistoryRecord.load()) {
            return timestamp;
        }
    }
}

void ThreadMonitorBase::maybeUpdateCentralRepository(
    std::chrono::system_clock::time_point timestamp) {
    if (timestamp - _lastCentralRepoUpdateTimestamp < _centralRepoUpdateInterval) {
        return;
    }
    _lastCentralRepoUpdateTimestamp = timestamp;
    _registration->lastSeenAlive = timestamp;
}

void ThreadMonitorBase::printHistory(const ThreadMonitorBase::History& history) {
    std::chrono::system_clock::time_point previous =
        history.empty() ? std::chrono::system_clock::time_point::min() : history[0].timestamp;
    for (const auto& h : history) {
        auto microsecs =
            std::chrono::duration_cast<std::chrono::microseconds>(h.timestamp.time_since_epoch()) %
            1000000;
        auto in_time_t = std::chrono::system_clock::to_time_t(h.timestamp);
        std::cerr << "Checkpoint: " << h.checkpointId
                  << " \tat: " << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X") << "."
                  << microsecs.count();
        std::cerr
            << "\tdelta: "
            << std::chrono::duration_cast<std::chrono::microseconds>(h.timestamp - previous).count()
            << " us";
#ifndef NDEBUG
        std::cerr << h.sequence;
#endif
        std::cerr << std::endl;
    }
}

}  // namespace details

void threadMonitorCheckpoint(uint32_t checkpointId) {
    auto* ptr = details::threadLocalPtr;
    if (ptr == nullptr) {
        return;
    }
    ptr->checkpointInternalImpl(checkpointId);
}

}  // namespace thread_monitor
