# Minimum CMake version
cmake_minimum_required(VERSION 3.27)

# Project name and languages
project(blang LANGUAGES C CXX)

# Source directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set_source_files_properties(${SRC_DIR}/clex.c PROPERTIES COMPILE_FLAGS $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-unused-function>)

# C++ source files
set(CPP_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/gen_ir.cpp
    ${SRC_DIR}/cli.cpp
    ${SRC_DIR}/clex_util.cpp
    ${SRC_DIR}/backend.cpp
)

# C source files
set(C_SOURCES
    ${SRC_DIR}/nob.c
    ${SRC_DIR}/clex.c
    ${SRC_DIR}/output.c
)

# Headers
file(GLOB HEADER_FILES
    "${SRC_DIR}/*.h"
    "${SRC_DIR}/*.hpp"
    "3rd-party/*.h"
    "3rd-party/*.hpp"
)

# executable for blang
add_executable(blang
    ${CPP_SOURCES}
    ${C_SOURCES}
    ${HEADER_FILES}
)

# compile options
target_compile_options(blang PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -ggdb>
    $<$<CXX_COMPILER_ID:Clang>:-g>
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /Zi>
)