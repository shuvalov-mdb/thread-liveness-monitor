#include "thread_monitor/thread_monitor.h"

#include <cassert>
#include <iostream>

namespace thread_monitor {

namespace details {

namespace {
thread_local ThreadMonitorBase *threadLocalPtr = nullptr;
} // namespace

ThreadMonitorBase::ThreadMonitorBase(std::string name,
                                     InternalHistoryRecord *historyPtr,
                                     uint32_t historyDepth,
                                     uint32_t firstCheckpointId)
    : _name(std::move(name)), _historyPtr(historyPtr),
      _historyDepth(historyDepth) {
  _maybeRegisterThreadLocal();
  checkpointInternalImpl(firstCheckpointId);
}

ThreadMonitorBase::~ThreadMonitorBase() {
  if (!_enabled) {
    return;
  }
  threadLocalPtr = nullptr;
}

bool ThreadMonitorBase::isEnabled() const { return _enabled; }

const std::string &ThreadMonitorBase::name() const { return _name; }

unsigned int ThreadMonitorBase::depth() const { return _historyDepth; }

void ThreadMonitorBase::_maybeRegisterThreadLocal() {
  if (threadLocalPtr != nullptr) {
    _enabled = false;
    return; // Not registering, previously registered up-stack.
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
    _historyPtr[0].durationFromCreation =
        std::chrono::system_clock::duration::zero();
#ifndef NDEBUG
    _historyPtr[0].sequence = ++_globalSequence;
#endif
    _headHistoryRecord = 0;
    _tailHistoryRecord = 0; // Inclusive.
    return;
  }
  // The circular buffer write is not atomic. 1. Advance the head if needed.
  unsigned int nextIndex;
  const bool tailCaughtHead =
      _headHistoryRecord.load() == _tailHistoryRecord.load() + 1 ||
      (_tailHistoryRecord.load() == _historyDepth - 1 &&
       _headHistoryRecord.load() == 0);
  if (tailCaughtHead) {
    nextIndex = _headHistoryRecord.load(); // Where head is now.
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
  writeCheckpointAtPosition(nextIndex, id);
  // 3. Advance the tail to point to the new record.
  _tailHistoryRecord = nextIndex;
}

void ThreadMonitorBase::writeCheckpointAtPosition(uint32_t index, uint32_t id) {
  assert(index >= 0);
  assert(index < _historyDepth);
  InternalHistoryRecord &r = *(_historyPtr + index);
  r.checkpointId = id;
  const auto now = std::chrono::system_clock::now();
  r.durationFromCreation = now - _creationTimestamp;
#ifndef NDEBUG
  r.sequence = ++_globalSequence; // Only in debug mode, very expensive.
#endif
}

ThreadMonitorBase::History ThreadMonitorBase::getHistory() const {
  ThreadMonitorBase::History history;
  // This code is not atomic. It is only guaranteed that the thread
  // monitor is protected from deletion.
  // TODO: add external deletion mutex.
  for (auto index = _headHistoryRecord.load();;) {
    HistoryRecord h;
    InternalHistoryRecord &r = *(_historyPtr + index);
    h.checkpointId = r.checkpointId;
    h.timestamp = _creationTimestamp + r.durationFromCreation.load();
#ifndef NDEBUG
    h.sequence = r.sequence;
#endif
    history.push_back(std::move(h));
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

} // namespace details

void threadMonitorCheckpoint(uint32_t checkpointId) {
  auto *ptr = details::threadLocalPtr;
  if (ptr == nullptr) {
    return;
  }
  ptr->checkpointInternalImpl(checkpointId);
}

} // namespace thread_monitor
