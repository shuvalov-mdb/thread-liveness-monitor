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
      writeCheckpointAtPosition(0, id);
  }
}

void ThreadMonitorBase::writeCheckpointAtPosition(uint32_t index, uint32_t id) {
  assert(index >= 0);
  assert(index < _historyDepth);
  InternalHistoryRecord& r = *(_historyPtr + index);
  r.checkpointId = id;
  const auto now = std::chrono::system_clock::now();
  //r.timestampMicrosFromEpoch = 
}

ThreadMonitorBase::History ThreadMonitorBase::getHistory() const {
    ThreadMonitorBase::History history;

    return history;
}

} // namespace details

} // namespace thread_monitor
