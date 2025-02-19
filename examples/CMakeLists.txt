# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(rgExamples)

# Add custom flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(EXAMPLES
    fib.cpp
    nqueens.cpp
    skynet.cpp
)

# Loop through each example and create an executable
foreach(EXAMPLE ${EXAMPLES})
    # Get the filename without extension to use as the target name
    get_filename_component(EXAMPLE_NAME ${EXAMPLE} NAME_WE)

    # Add the executable and link the library
    add_executable(${EXAMPLE_NAME} ${EXAMPLE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE rg)
endforeach()
