cmake_minimum_required(VERSION 3.16)
project(macroitems_cpp LANGUAGES CXX)

include(CheckIPOSupported)

# The implementation uses C++20 for small conveniences such as structured
# bindings.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MACROITEMS_ENABLE_IPO "Enable interprocedural optimization/LTO when supported" ON)
option(MACROITEMS_ENABLE_NATIVE_ARCH "Compile for the native CPU architecture" ON)

# HiGHS is vendored under third_party/HiGHS.  If the source tree lives elsewhere,
# configure with -DHIGHS_SOURCE_DIR=/path/to/HiGHS.
set(MACROITEMS_DEFAULT_HIGHS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/HiGHS")
if((NOT DEFINED HIGHS_SOURCE_DIR OR NOT EXISTS "${HIGHS_SOURCE_DIR}/CMakeLists.txt")
    AND EXISTS "${MACROITEMS_DEFAULT_HIGHS_SOURCE_DIR}/CMakeLists.txt")
    set(HIGHS_SOURCE_DIR "${MACROITEMS_DEFAULT_HIGHS_SOURCE_DIR}"
        CACHE PATH "Path to the HiGHS source tree" FORCE)
else()
    set(HIGHS_SOURCE_DIR "${MACROITEMS_DEFAULT_HIGHS_SOURCE_DIR}"
        CACHE PATH "Path to the HiGHS source tree")
endif()
if(NOT EXISTS "${HIGHS_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "HiGHS source tree not found at ${HIGHS_SOURCE_DIR}. "
        "Set HIGHS_SOURCE_DIR to the HiGHS source directory, or place HiGHS "
        "under third_party/HiGHS.")
endif()

# We only need the HiGHS library, not its executable, examples, or tests.
set(BUILD_CXX_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_CXX_EXAMPLE OFF CACHE BOOL "" FORCE)
set(ZLIB OFF CACHE BOOL "" FORCE)
add_subdirectory("${HIGHS_SOURCE_DIR}" third_party/HiGHS-build EXCLUDE_FROM_ALL)

# Core library — one translation unit per concern:
#   core          : core data structures and inline ratio comparison
#   instance      : graph checks, validation, paper example, stream printing
#   lp_highs      : HiGHS wrapper (solve_max_lp)
#   pckp_lp       : direct PCKP LP relaxation solved via HiGHS
#   solution      : primal solution induced by a macroitem sequence
#   result_writer : text result files and timing output
#   yed_writer     : yEd GraphML graph visualizations
#   algo_forest   : heap-based directed-forest contraction
#   algo_forest_noheap : Algorithm 1-style directed-forest contraction without heaps
#   algo_in_forest  : Algorithm 2 — O(n log n) in-tree max-heap
#   algo_out_forest : Algorithm 3 — O(n log n) out-tree min-heap
#   algo_dinkelbach : Algorithm 4 — Lagrangian/Dinkelbach closure-LP via HiGHS
add_library(macroitems
    src/instance.cpp
    src/lp_highs.cpp
    src/pckp_lp.cpp
    src/solution.cpp
    src/result_writer.cpp
    src/yed_writer.cpp
    src/algo_forest.cpp
    src/algo_forest_noheap.cpp
    src/algo_in_forest.cpp
    src/algo_out_forest.cpp
    src/algo_dinkelbach.cpp
)
# Public headers live under include/; src/ is added privately so that the
# internal helpers under src/internal/ are resolvable from source files.
target_include_directories(macroitems PUBLIC include PRIVATE src)
target_link_libraries(macroitems PUBLIC highs)
target_compile_options(macroitems PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-DNDEBUG>
)
if(MACROITEMS_ENABLE_NATIVE_ARCH AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(macroitems PRIVATE $<$<CONFIG:Release>:-march=native>)
endif()
if(MACROITEMS_ENABLE_IPO)
    check_ipo_supported(RESULT macroitems_ipo_supported OUTPUT macroitems_ipo_error LANGUAGES CXX)
    if(macroitems_ipo_supported)
        set_property(TARGET macroitems PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(STATUS "IPO/LTO disabled: ${macroitems_ipo_error}")
    endif()
endif()

# Regression test executable. It intentionally has no external test framework,
# so it can be run anywhere the project builds.
add_executable(macroitems_tests tests/macroitems_tests.cpp)
target_link_libraries(macroitems_tests PRIVATE macroitems)

# Utility regression checker for large instance sets: compares the heap-based
# and no-heap forest algorithms directly in memory.
add_executable(forest_noheap_compare tests/forest_noheap_compare.cpp)
target_link_libraries(forest_noheap_compare PRIVATE macroitems)

# Command-line driver: reads an instance, validates compatibility, runs one of
# the algorithms, and writes the result file.
# Usage: macroitems_main <instance_file> <algorithm> <output_file> [capacity]
add_executable(macroitems_main examples/macroitems_main.cpp)
target_link_libraries(macroitems_main PRIVATE macroitems)
if(MACROITEMS_ENABLE_IPO AND macroitems_ipo_supported)
    set_property(TARGET macroitems_main PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
