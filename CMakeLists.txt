cmake_minimum_required(VERSION 3.10)
project(BacktestingEngine LANGUAGES CXX)

# Force GCC to use C++17 or C++20 for modern features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Include Directories: This lets you use #include "portfolio/header.hpp"
include_directories(.)

# 2. Gather your source files
# Professional Tip: Explicitly list files instead of using GLOB 
# to avoid build issues when adding/removing files.
set(SOURCES
    main.cpp
    portfolio/portfolio.cpp
    execution/execution.cpp
    data_handler/data_handler.cpp
    # Add other .cpp files here
)

# 3. Create the executable
add_executable(Backtester ${SOURCES})

# 4. Compiler-specific flags for GCC
if(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(Backtester PRIVATE -Wall -Wextra -O3)
endif()