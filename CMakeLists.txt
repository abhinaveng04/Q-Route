cmake_minimum_required(VERSION 3.16)
project(qroute LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(QRoute_BUILD_TESTS "Build unit tests" ON)
option(QRoute_BUILD_BENCHMARKS "Build benchmarks" ON)
option(QRoute_WERROR "Warnings as errors" OFF)
option(QRoute_SANITIZE "Enable ASan/UBSan" OFF)

if(MINGW)
    add_link_options(-static)
endif()

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

enable_testing()

add_custom_target(check_invariants
    COMMAND ${CMAKE_COMMAND} -E echo "Running architectural invariant scanner..."
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_invariants.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_subdirectory(src)
add_subdirectory(apps)

if(QRoute_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(QRoute_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()