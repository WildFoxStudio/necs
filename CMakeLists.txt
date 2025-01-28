# Minimum required version of CMake
cmake_minimum_required(VERSION 3.0)

# Project name and language
project(necs)

# Set the library source files
set(SOURCES
    include/necs/CEntityFactory.h
    include/necs/CMatrixAllocator.h
    include/necs/CPagedAllocator.h
    include/necs/CWorldObject.h
    include/necs/IAllocator.h
    include/necs/IAlignedAllocator.h
    include/necs/IEntityFactory.h
    include/necs/IIDGenerator.h
    include/necs/IPagedAllocator.h
    include/necs/IWorldObjectCDO.h

    src/necs/CWorldObject.cpp
)

# Add the library target
# Use STATIC for a static library, SHARED for a shared library
add_library(necs STATIC ${SOURCES})

# Specify the include directory for this library
target_include_directories(necs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Fetch clow from GitHub
include(FetchContent) # once in the project to include the module
FetchContent_Declare(
    clow
    GIT_REPOSITORY https://github.com/WildFoxStudio/clow.git
    GIT_TAG main
    )
    
# Fetch the content and add it to your CMake project
FetchContent_MakeAvailable(clow)
    
# Link against the clow library
target_link_libraries(necs PRIVATE clow)

# Optionally set the C++ standard (e.g., C++17)
set_target_properties(necs PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
add_subdirectory(tests)
endif()