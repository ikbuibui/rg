cmake_minimum_required(VERSION 3.10)
project(rg VERSION 0.1.0 LANGUAGES CXX)

# Set build type
set(CMAKE_BUILD_TYPE Debug)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add custom flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find Boost for header only boost.lockfree
# find_package(Boost REQUIRED)
# include_directories(${Boost_INCLUDE_DIRS})

# Include the header files
include_directories(src)

# Add the library target (header-only)
add_library(rg INTERFACE)
target_include_directories(rg INTERFACE include)

# Add subdirectory for tests
add_subdirectory(tests)
