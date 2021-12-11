/**
MIT License

Copyright (c) 2021 MongoDB, Inc; Author Andrew Shuvalov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#include "thread_monitor/thread_monitor_central_repository.h"

namespace thread_monitor {

/**
 * Method to instrument the code with checkpoints.
 * A thread is considered alive if it last called this method within the
 * 'thread timeout' in the past. This timeout can be configured with
 * `setThreadTimeout()` on the ThreadMonitorCentralRepository instance.
 * The default value is 5 minutes.
 */
void threadMonitorCheckpoint(uint32_t checkpointId);

namespace details {

/** Documentation: https://github.com/shuvalov-mdb/thread-liveness-monitor
 */
class ThreadMonitorBase {
public:
    struct InternalHistoryRecord {
        std::atomic<uint32_t> checkpointId;
        std::atomic<std::chrono::system_clock::duration> durationFromCreation;

#ifndef NDEBUG
        // Sequence number is very expensive to generate and thus
        // it should be used only with debug builds.
        std::atomic<uint64_t> sequence;
#endif
    };

    /**
     * Represents one checkpoint visited by this monitor.
     */
    struct HistoryRecord {
        uint32_t checkpointId;
        std::chrono::system_clock::time_point timestamp;

#ifndef NDEBUG
        // Sequence number is very expensive to generate and thus
        // it should be used only with debug builds.
        uint64_t sequence;
#endif
    };

    using History = std::vector<HistoryRecord>;

    bool isEnabled() const;

    const char* name() const;

    unsigned int depth() const;

    /**
     * Returns the snapshot of History, which is the vector of
     * recently visited checkpoints. The count of checkpoints preserved
     * in history is parameterized as HistoryDepth below. There is no
     * performance penalty for keeping longer history, but the summary stack
     * traces can become unnecessary cluttered.
     */
    History getHistory() const;

    /**
     * Returns the timestamp of the last checkpoint visited.
     */
    std::chrono::system_clock::time_point lastCheckpointTime() const;

    void printHistory() const;
    static void printHistory(const History& history);

protected:
    ThreadMonitorBase(const char* const name,
                      InternalHistoryRecord* historyPtr,
                      uint32_t historyDepth,
                      uint32_t firstCheckpointId);
    // The inheritance is non-virtual as the instance of this class can exist
    // only on the stack and the destructor by the pointer of the base class
    // cannot be invoked.
    ~ThreadMonitorBase();

    ThreadMonitorBase(const ThreadMonitorBase&) = delete;
    ThreadMonitorBase& operator=(const ThreadMonitorBase&) = delete;

    /**
     * Register a checkpoint with 'id' for this monitor, if enabled.
     * This is internal implementation to be accessed from
     * threadMonitorCheckpoint().
     */
    void checkpointInternalImpl(uint32_t id);

    void writeCheckpointAtPosition(uint32_t index,
                                   uint32_t id,
                                   std::chrono::system_clock::time_point timestamp);

    // We only update the central repository once in a while, for performance.
    void maybeUpdateCentralRepository(std::chrono::system_clock::time_point timestamp);

private:
    friend void ::thread_monitor::threadMonitorCheckpoint(uint32_t checkpointId);

    void _maybeRegisterThreadLocal();

    // Thread name, the pointer should remain valid for the lifetime.
    const char* const _name;
    InternalHistoryRecord* const _historyPtr;
    const uint32_t _historyDepth;

    const std::chrono::system_clock::time_point _creationTimestamp =
        std::chrono::system_clock::now();
    const std::thread::id _threadId = std::this_thread::get_id();

    // Thread monitor is disabled if there is another instance up the stack.
    bool _enabled = false;
    // History is not guarded by a mutex. Instead, the update sequence is:
    // 1. Advance head if the list is full
    // 2. Insert new record (possibly where the head was before)
    // 3. Advance tail
    // Thus non-atomic value insertion happens outside of head-tail interval.
    std::atomic<uint32_t> _headHistoryRecord = _historyDepth;
    std::atomic<uint32_t> _tailHistoryRecord = _historyDepth;

    // Prorate updates to central repository to avoid cache misses.
    std::chrono::system_clock::time_point _lastCentralRepoUpdateTimestamp = _creationTimestamp;
    std::chrono::system_clock::duration _centralRepoUpdateInterval;

    ThreadMonitorCentralRepository::ThreadRegistration* _registration;

#ifndef NDEBUG
    static std::atomic<uint64_t> _globalSequence;
#endif
};
}  // namespace details

/**
 * RAII class ThreadMonitor that registers (in the constructor) and
 * deregisters (in the destructor) the current thread in the central repository
 * and enables the instrumentation by placing the calls to 
 * `threadMonitorCheckpoint()` anywhere in the code.
 * 
 * `HistoryDepth` is how many checkpoints are stored on this class.
 * 
 * Important: ThreadMonitor is designed to be used as automatic instance within
 * a method scope. It must be deleted by the same thread that created it, otherwise
 * it will not deregister the thread local variable pointing to it and will
 * corrupt memory.
 */
template <uint32_t HistoryDepth = 10>
class ThreadMonitor : public details::ThreadMonitorBase {
public:
    /**
     * @param name Thread name, the pointer should remain valid for the lifetime.
     * @param firstCheckpointId the checkpoint id for the registration checkpoint.
     */
    ThreadMonitor(const char* const name,
                  uint32_t firstCheckpointId);

private:
    // The actual history circular list is stored on stack.
    InternalHistoryRecord _history[HistoryDepth];
};

template <uint32_t HistoryDepth>
ThreadMonitor<HistoryDepth>::ThreadMonitor(const char* const name,
                                           uint32_t firstCheckpointId)
    : ThreadMonitorBase(name, _history, HistoryDepth, firstCheckpointId) {}

}  // namespace thread_monitor
