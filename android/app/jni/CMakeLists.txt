# Builds libmain.so: XU4 with SDL3 & Boron linked in.  XU4_DEPS is the
# directory holding the SDL & boron sources (set up by
# dist/build-android.sh).

cmake_minimum_required(VERSION 3.22)
project(xu4 C CXX)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${XU4_DEPS}/SDL SDL)

# The Boron options of the other builds (--thread --no-socket).
set(BORON_DEFS CONFIG_CHECKSUM CONFIG_COMPRESS=1 CONFIG_HASHMAP CONFIG_RANDOM
    CONFIG_STATIC CONFIG_THREAD)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/xu4.cmake)

add_library(main SHARED ${XU4_SRCS} ${XU4_SRC}/android.cpp)
xu4_target(main)
target_link_libraries(main PRIVATE SDL3::SDL3-static boron GLESv3 EGL log
                      android z m)
