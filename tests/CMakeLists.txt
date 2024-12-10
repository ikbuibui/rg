# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(rgTest)

# Add custom flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-g -fcoroutines")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fcoroutines -DNDEBUG")

# Find Catch2
# include(FetchContent)
# FetchContent_Declare(
#  catch2
#  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#  GIT_TAG v3.7.1)
# FetchContent_MakeAvailable(catch2)

# The following is borrowed from:
# https://github.com/google/boringssl/blob/master/CMakeLists.txt

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CLANG 1)
endif()

# set(CFI 1)

# set(TSAN 1)

# set(MSAN 1)

# set(UBSAN 1)

# set(ASAN 1)

if(MSAN)
  if(ASAN)
    message(FATAL_ERROR "ASAN and MSAN are mutually exclusive")
  endif()

  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer"
  )
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer"
  )
  set(CMAKE_ASM_FLAGS
      "${CMAKE_ASM_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer"
  )
endif()

if(ASAN)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer"
  )
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer"
  )
endif()

if(CFI)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable CFI unless using Clang")
  endif()

  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin -fvisibility=hidden"
  )
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin -fvisibility=hidden"
  )
endif()

if(TSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable TSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

if(UBSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable UBSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")

  if(NOT UBSAN_RECOVER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-sanitize-recover=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=undefined")
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -fno-sanitize-recover=undefined")
  endif()
endif()

set(TEST_SOURCES # test_rg.cpp
    # randomGraph.cpp
    randGraph.cpp)

set(TEST_TARGET rg_tests)

# Create the test executable
add_executable(${TEST_TARGET} ${TEST_SOURCES})

# Link the library and Catch2

# target_link_libraries(${TEST_TARGET} PRIVATE rg Catch2::Catch2WithMain)
target_link_libraries(${TEST_TARGET} PRIVATE rg)

# Enable CTest

# include(CTest) include(Catch) catch_discover_tests(rg_tests)
