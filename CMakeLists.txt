cmake_minimum_required(VERSION 3.14)
project(BacktestingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(.)

# ── doctest (vendored header — no network required) ───────────────────────────
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_SOURCE_DIR}/third_party")
add_library(doctest::doctest ALIAS doctest)

# ── Protobuf ──────────────────────────────────────────────────────────────────
find_package(Protobuf REQUIRED)

# Generate ohlcv.pb.cc / ohlcv.pb.h from the proto schema.
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/ohlcv.proto)

find_package(Threads REQUIRED)

# ── Engine library (all components, no main.cpp) ──────────────────────────────
# Splitting engine code into a static lib lets both the Backtester binary and
# the test executables link against it without recompiling every source twice.
set(ENGINE_SOURCES
    src/event_queue.cpp
    src/data_handler.cpp
    src/proto_data_handler.cpp
    src/strategy/strategy.cpp
    src/strategy/rsi_strategy.cpp
    src/strategy/mean_reversion_strategy.cpp
    src/strategy/momentum_strategy.cpp
    src/strategy/scalping_strategy.cpp
    src/strategy/breakout_strategy.cpp
    src/strategy/pullback_strategy.cpp
    src/strategy/vwap_strategy.cpp
    src/strategy/orb_strategy.cpp
    src/strategy/strategy_factory.cpp
    src/portfolio.cpp
    src/execution_handler.cpp
    src/performance.cpp
    src/live_session.cpp
    src/run_backtest.cpp
    ${PROTO_SRCS}
)

add_library(engine_lib STATIC ${ENGINE_SOURCES})
target_include_directories(engine_lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"   # where ohlcv.pb.h is generated
    "${Protobuf_INCLUDE_DIRS}"      # google/protobuf/*.h (runtime_version.h etc.)
)
target_link_libraries(engine_lib PUBLIC ${Protobuf_LIBRARIES} Threads::Threads)

if(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(engine_lib PRIVATE -Wall -Wextra -O2)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(engine_lib PRIVATE -Wall -Wextra -O2)
endif()

# ── Backtester executable ─────────────────────────────────────────────────────
add_executable(Backtester main.cpp)
target_link_libraries(Backtester PRIVATE engine_lib)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
