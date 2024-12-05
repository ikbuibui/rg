cmake_minimum_required(VERSION 3.12)

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
            $<$<CXX_COMPILER_ID:Clang>:-Weverything>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# Add subdirectory for tests
add_subdirectory(tests)

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

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)
