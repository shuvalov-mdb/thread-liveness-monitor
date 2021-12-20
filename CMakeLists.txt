cmake_minimum_required(VERSION 3.11)
project(thread-liveness-monitor)

include_directories(src)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src/thread_monitor build)

enable_testing()

add_test(NAME thread_monitor_test COMMAND thread_monitor_test)
