cmake_minimum_required(VERSION 3.10)
project(rg2 VERSION 0.1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find Boost for header only boost.lockfree
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Include the header files
include_directories(src)

# Add the library target (header-only)
add_library(rg2 INTERFACE)
target_include_directories(rg2 INTERFACE include)

# Add subdirectory for tests
add_subdirectory(tests)
