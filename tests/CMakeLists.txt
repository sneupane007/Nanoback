# tests/CMakeLists.txt
# Each component gets its own test executable so failures are isolated.
# All targets link engine_lib (the static library built in the root CMakeLists).

function(add_engine_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE engine_lib doctest::doctest)
    target_include_directories(${name} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}"
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_engine_test(test_event_queue          events/test_event_queue.cpp)
add_engine_test(test_data_handler         data/test_data_handler.cpp)
add_engine_test(test_proto_data_handler   data/test_proto_data_handler.cpp)
add_engine_test(test_strategy             strategy/test_strategy.cpp)
add_engine_test(test_portfolio            portfolio/test_portfolio.cpp)
add_engine_test(test_execution            execution/test_execution_handler.cpp)
add_engine_test(test_performance          performance/test_performance.cpp)
add_engine_test(test_live_session         live/test_live_session.cpp)
add_engine_test(test_run_backtest         run_backtest/test_run_backtest.cpp)
