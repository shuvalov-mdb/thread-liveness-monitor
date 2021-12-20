add_library (thread-liveness-monitor thread_monitor.cpp thread_monitor_central_repository.cpp)

target_include_directories(thread-liveness-monitor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(
    thread_monitor_test
    thread_monitor_test.cpp
)

target_link_libraries(
    thread_monitor_test
    pthread
)

target_include_directories(thread_monitor_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(
    thread_monitor_test
    thread-liveness-monitor
    gtest_main
    gtest
)

include(GoogleTest)
gtest_discover_tests(thread_monitor_test)
