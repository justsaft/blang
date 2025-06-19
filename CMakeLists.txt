# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and languages
project(blang LANGUAGES C CXX)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

# Source directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(BLD_DIR "${CMAKE_BINARY_DIR}/build")

# C++ source files
set(CPP_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/gen_ir.cpp
    ${SRC_DIR}/state.cpp
    ${SRC_DIR}/cli.cpp
    ${SRC_DIR}/clex_util.cpp
)

# C source files
set(C_SOURCES
    ${SRC_DIR}/nob.c
    ${SRC_DIR}/clex.c
    ${SRC_DIR}/output.c
)

# Add executable for blang
add_executable(blang
    ${CPP_SOURCES}
    ${C_SOURCES}
)

# Set C++ standard for target.cpp (uses C++11)
set_source_files_properties(${SRC_DIR}/target.cpp PROPERTIES COMPILE_FLAGS "-std=c++11")

# Set compile options for C and C++
target_compile_options(blang PRIVATE -Wall -Wextra -ggdb)

# For clex.c, suppress -Wunused-function
set_source_files_properties(${SRC_DIR}/clex.c PROPERTIES COMPILE_FLAGS "-Wno-unused-function")

# Custom target for nob (bootstrap)
add_executable(nob ${CMAKE_SOURCE_DIR}/build.c)
set_target_properties(nob PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BLD_DIR}
)

# Add a custom target to mimic 'nob_build'
add_custom_target(nob_build
    COMMAND ${BLD_DIR}/nob
    DEPENDS nob
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running nob bootstrap build"
)

# Add a FORCE target (dummy, for compatibility)
add_custom_target(FORCE
    COMMENT "Dummy FORCE target for compatibility"
)

# Ensure build directory exists (CMake handles this by default)
# No need for explicit mkdir

# Notes:
# - To build blang: `cmake --build . --target blang`
# - To build nob:   `cmake --build . --target nob`
# - To run nob_build: `cmake --build . --target nob_build`