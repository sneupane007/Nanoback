cmake_minimum_required(VERSION 3.14)
project(BacktestingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(.)

# ── Engine library (all components, no main.cpp) ──────────────────────────────
# Splitting engine code into a static lib lets both the Backtester binary and
# the test executables link against it without recompiling every source twice.
set(ENGINE_SOURCES
    src/event_queue.cpp
    src/data_handler.cpp
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
)

add_library(engine_lib STATIC ${ENGINE_SOURCES})
target_include_directories(engine_lib PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}")

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
# FetchContent pulls doctest at configure time — no manual download needed.
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

enable_testing()
add_subdirectory(tests)

# ── PyBind11 Python extension module ─────────────────────────────────────────
# FetchContent pulls pybind11 at configure time — no manual install needed.
# The output is a shared library (e.g. nanoback.cpython-311-darwin.so) that
# Python's import system picks up when build/ is on sys.path.
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.1
)
FetchContent_MakeAvailable(pybind11)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

if(EXISTS "${CMAKE_SOURCE_DIR}/bindings/nanoback_bindings.cpp")
    pybind11_add_module(nanoback bindings/nanoback_bindings.cpp)
    target_link_libraries(nanoback PRIVATE engine_lib)
    target_include_directories(nanoback PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}"
    )
endif()
