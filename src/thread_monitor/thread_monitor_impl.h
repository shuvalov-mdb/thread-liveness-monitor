namespace thread_monitor {

template <unsigned int HistoryDepth,
          typename TimeSupport>
class ThreadMonitor;

namespace details {
struct ThreadMonitorThreadLocalInterface {

};

extern bool maybeRegisterThreadLocal(ThreadMonitorThreadLocalInterface* tli);

}  // namespace details



}  // namespace thread_monitor
