cmake_minimum_required(VERSION 3.13)
project(infinite_drive C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# raylib 5.5 declares an old minimum version; allow it on CMake >= 4
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

find_package(raylib 5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib
    GIT_TAG 6.0)
  FetchContent_MakeAvailable(raylib)
endif()

file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_executable(infinite-drive ${SRC})

if(raylib_FOUND)
  target_link_libraries(infinite-drive PRIVATE raylib::raylib)
else()
  target_link_libraries(infinite-drive PRIVATE raylib)
  target_include_directories(infinite-drive PRIVATE ${raylib_SOURCE_DIR}/src)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(infinite-drive PRIVATE -Wall -Wextra -DGL_SILENCE_DEPRECATION)
endif()
