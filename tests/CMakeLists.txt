# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# Find Catch2
include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1 # Specify the version you want
)
FetchContent_MakeAvailable(catch2)

# Create the test executable
add_executable(rg2_tests test_rg2.cpp)

# Link the library and Catch2
target_link_libraries(rg2_tests PRIVATE rg2 Catch2::Catch2WithMain)

# Enable CTest
include(CTest)
include(Catch)
catch_discover_tests(rg2_tests)
