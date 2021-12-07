#include <atomic>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>

namespace thread_monitor {

void threadMonitorCheckpoint(uint32_t checkpointId);

namespace details {
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

  const std::string &name() const;

  unsigned int depth() const;

  /**
   * Returns the snapshot of History, which is the vector of
   * recently visited checkpoints.
   */
  History getHistory() const;

protected:
  ThreadMonitorBase(std::string name, InternalHistoryRecord *historyPtr,
                    uint32_t historyDepth, uint32_t firstCheckpointId);
  // The inheritance is non-virtual as the instance of this class can exist
  // only on the stack and the destructor by the pointer of the base class
  // cannot be invoked.
  ~ThreadMonitorBase();

  ThreadMonitorBase(const ThreadMonitorBase &) = delete;
  ThreadMonitorBase &operator=(const ThreadMonitorBase &) = delete;

protected:
  /**
   * Register a checkpoint with 'id' for this monitor, if enabled.
   * This is internal implementation to be accessed from
   * threadMonitorCheckpoint().
   */
  void checkpointInternalImpl(uint32_t id);

  void writeCheckpointAtPosition(uint32_t index, uint32_t id);

private:
  friend void ::thread_monitor::threadMonitorCheckpoint(uint32_t checkpointId);

  void _maybeRegisterThreadLocal();

  const std::string _name;
  InternalHistoryRecord *const _historyPtr;
  const uint32_t _historyDepth;

  const std::chrono::system_clock::time_point _creationTimestamp =
      std::chrono::system_clock::now();

  // Thread monitor is disabled if there is another instance up the stack.
  bool _enabled = false;
  // History is not guarded by a mutex. Instead, the update sequence is:
  // 1. Advance head if the list is full
  // 2. Insert new record (possibly where the head was before)
  // 3. Advance tail
  // Thus non-atomic value insertion happens outside of head-tail interval.
  std::atomic<uint32_t> _headHistoryRecord = _historyDepth;
  std::atomic<uint32_t> _tailHistoryRecord = _historyDepth;

#ifndef NDEBUG
  std::atomic<uint64_t> _globalSequence{0};
#endif
};
} // namespace details

template <uint32_t HistoryDepth = 10>
class ThreadMonitor : public details::ThreadMonitorBase {
public:
  ThreadMonitor(std::string name, uint32_t firstCheckpointId);

private:
  // The actual history circular list is stored on stack.
  InternalHistoryRecord _history[HistoryDepth];
};

template <uint32_t HistoryDepth>
ThreadMonitor<HistoryDepth>::ThreadMonitor(std::string name,
                                           uint32_t firstCheckpointId)
    : ThreadMonitorBase(std::move(name), _history, HistoryDepth,
                        firstCheckpointId) {}

} // namespace thread_monitor
