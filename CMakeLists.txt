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
# Module mode (FindProtobuf) is required for protobuf_generate_cpp() below — the
# CONFIG package does not define it. On protobuf v5+ the generated code's CHECK
# macros instantiate Abseil logging templates, so Abseil must be found and linked
# explicitly (module-mode protobuf::libprotobuf does not carry it).
find_package(Protobuf REQUIRED)
find_package(absl CONFIG REQUIRED)

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
# protobuf::libprotobuf for the runtime; the two absl log targets satisfy the
# CHECK-macro symbols instantiated in the generated ohlcv.pb.cc (their interface
# deps pull in the rest of Abseil). Without these, a clean build fails to link
# with undefined absl::lts_*::log_internal::* symbols.
target_link_libraries(engine_lib PUBLIC
    protobuf::libprotobuf
    absl::log_internal_check_op
    absl::log_internal_message
    Threads::Threads)

if(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(engine_lib PRIVATE -Wall -Wextra -O2)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(engine_lib PRIVATE -Wall -Wextra -O2)
endif()

# ── Backtester executable ─────────────────────────────────────────────────────
# Skipped under scikit-build-core (SKBUILD is set): a `pip install .` build only
# needs the `nanoback` extension module below, not the standalone CLI binary.
if(NOT SKBUILD)
    add_executable(Backtester main.cpp)
    target_link_libraries(Backtester PRIVATE engine_lib)
endif()

# ── Python bindings (optional) ────────────────────────────────────────────────
# Built only when BOTH the binding source is present AND pybind11 is findable, so
# a fresh clone without the module (or without pybind11 installed) still builds
# the default targets. To enable: install pybind11 and pass its cmake dir, e.g.
#   cmake -B build -Dpybind11_DIR=$(python -m pybind11 --cmakedir) \
#                  -DPython_EXECUTABLE=$(which python)
if(EXISTS "${CMAKE_SOURCE_DIR}/bindings/nanoback_bindings.cpp")
    find_package(pybind11 CONFIG QUIET)
    if(pybind11_FOUND)
        pybind11_add_module(nanoback bindings/nanoback_bindings.cpp)
        target_link_libraries(nanoback PRIVATE engine_lib)
        install(TARGETS nanoback DESTINATION .)
        message(STATUS "pybind11 ${pybind11_VERSION} found — building 'nanoback' Python module")
    else()
        message(STATUS "pybind11 not found — skipping 'nanoback' Python module")
    endif()
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(NOT SKBUILD)
    enable_testing()
    add_subdirectory(tests)
endif()
