cmake_minimum_required(VERSION 3.5)
project(necs_tests LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Fetch
set(gtest_force_shared_crt on)
include(FetchContent) # once in the project to include the module
FetchContent_Declare(
    gtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
    )
# Fetch the content and add it to your CMake project
FetchContent_MakeAvailable(gtest)
enable_testing()

# Include directories
include_directories("../include")

# Tests
add_executable(necs_tests src/main.cpp)
target_include_directories(necs_tests PUBLIC "../include")

target_link_libraries(necs_tests PRIVATE necs clow gtest gmock)