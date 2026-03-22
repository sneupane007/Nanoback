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
    src/events/events.cpp
    src/events/event_queue.cpp
    src/data/data_handler.cpp
    src/strategy/strategy.cpp
    src/portfolio/portfolio.cpp
    src/execution/execution_handler.cpp
    src/performance/performance.cpp
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
