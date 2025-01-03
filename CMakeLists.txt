cmake_minimum_required(VERSION 3.13)

project(
  "rg"
  VERSION 0.1.0
  LANGUAGES CXX
  HOMEPAGE_URL "https://github.com/ikbuibui/rg")

include(GNUInstallDirs)

# Set build type
set(CMAKE_BUILD_TYPE Release)

# Find Boost for header only boost.lockfree

# find_package(Boost REQUIRED)

# include_directories(${Boost_INCLUDE_DIRS})

# Add the library target (header-only)
add_library(rg INTERFACE)
target_include_directories(
  rg INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Specify the C++ standard
target_compile_features(rg INTERFACE cxx_std_20)

# Add specific compiler options for the library
target_compile_options(
  rg
  INTERFACE $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -fcoroutines>
            $<$<CXX_COMPILER_ID:Clang>:-Weverything
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-c++98-c++11-compat-pedantic>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>)

find_package(Threads REQUIRED)

# Find HwLoc
find_path(
  HWLOC_INCLUDE_DIR
  NAMES hwloc.h
  PATHS /opt/local
        /usr/local
        /usr
        ENV
        "PROGRAMFILES(X86)"
        ENV
        "HWLOC_ROOT"
  PATH_SUFFIXES include)

find_library(
  HWLOC
  NAMES libhwloc.lib hwloc
  PATHS ENV "HWLOC_ROOT"
  PATH_SUFFIXES lib)

if(HWLOC STREQUAL "HWLOC-NOTFOUND" OR ${HWLOC_INCLUDE_DIR} STREQUAL
                                      "HWLOC_INCLUDE_DIR-NOTFOUND")
  message(
    FATAL_ERROR
      "hwloc NOT found: use `-DHWLOC_ENABLE=OFF` to build without hwloc support"
  )
else()
  message(STATUS "Found hwloc")
endif()
# Use get_filename_component to extract the directory from the HWLOC library
# path
get_filename_component(HWLOC_LIBRARY_DIR ${HWLOC} DIRECTORY)

# Add the library directory to the link directories
target_link_directories(rg INTERFACE ${HWLOC_LIBRARY_DIR})

target_link_libraries(rg INTERFACE Threads::Threads ${HWLOC})

target_include_directories(rg INTERFACE ${HWLOC_INCLUDE_DIR})

# Examples & Tests

option(BUILD_EXAMPLES "Build the examples" OFF)
option(BUILD_TESTING "Build the tests" OFF)

if(BUILD_EXAMPLES)
  add_subdirectory("examples/")
endif()

# Add subdirectory for tests
if(BUILD_TESTING)
  add_subdirectory("tests/")
endif()

# Installation
install(
  TARGETS rg
  EXPORT ${PROJECT_NAME}_Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "rgConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(
  EXPORT ${PROJECT_NAME}_Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include/${PROJECT_NAME} )
