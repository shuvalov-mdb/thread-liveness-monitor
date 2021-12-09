# Thread Liveness Monitor
------------

TLM is production ready C++ library designed to detect deadlocks, livelocks and
starvation at runtime with tiny overhead. The project is sponsored and copyrighted by [MongoDB.com](http://mongodb.com), author [Andrew Shuvalov](https://www.linkedin.com/in/andrewshuvalov/).

## Sample Usage
  ```
  #include "thread_monitor/thread_monitor.h"

  void myLivelockedMethod();

  void myParentMethod() {
    thread_monitor::ThreadMonitor<> monitor("Livelock demo", 1);

    std::this_thread::sleep_for(2ms);
    thread_monitor::threadMonitorCheckpoint(2);
    myLivelockedMethod();
  }

  void myLivelockedMethod() {
    thread_monitor::threadMonitorCheckpoint(3);

    while (true) {
        std::this_thread::sleep_for(1ms);
    }
  }

  ```


  After the timeout configured with `ThreadMonitorCentralRepository::setThreadTimeout()` expires the library will dump the checkpoint history for the stuck thread and then for all other instrumented threads that are stuck for more than *1 millis* (to reduce verbosity):

  ```
Frozen thread: Livelock demo id: 140085845083904
Checkpoint: 1   at: 2021-12-09 23:29:36.201542  delta: 0 us
Checkpoint: 2   at: 2021-12-09 23:29:36.203625  delta: 2083 us
  ```

and then it will invoke the callback registered with `ThreadMonitorCentralRepository::setLivenessErrorConditionDetectedCallback()`. Most likely, you would like to terminate your program when this callback is called.

- Note: if you add a `threadMonitorCheckpoint()` inside the `while()` loop   above, the thread will be considered alive and the *liveness error* will not be triggered.

