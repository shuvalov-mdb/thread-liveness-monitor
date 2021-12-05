#include "thread_monitor/thread_monitor.h"

namespace thread_monitor {

namespace details {

bool maybeRegisterThreadLocal(ThreadMonitorThreadLocalInterface* tli) {
    return true;
}

}  // namespace details


}  // namespace thread_monitor
