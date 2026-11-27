cmake_minimum_required(VERSION 3.25)

# Static MSVC runtime (must be set before any target is created; CMP0091).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(FreeFrame VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FREEFRAME_BUILD_TESTS "Build FreeFrame unit tests" ON)
option(FREEFRAME_WARNINGS_AS_ERRORS "Treat warnings as errors in FreeFrame targets" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

if(FREEFRAME_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(core)
add_subdirectory(cli)
if(FREEFRAME_BUILD_TESTS)
  add_subdirectory(tests)
endif()
