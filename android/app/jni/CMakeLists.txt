# Builds libmain.so: XU4 with SDL3 & Boron linked in.  The xu4 source list
# is read from src/Makefile.common so that it stays the same as the other
# builds.  XU4_DEPS is the directory holding the SDL & boron sources (set
# up by dist/build-android.sh).

cmake_minimum_required(VERSION 3.22)
project(xu4 C CXX)

set(XU4_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../src)

# SDL3, static.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${XU4_DEPS}/SDL SDL)

# Boron, static, with the options of the other builds (--thread --no-socket).
set(BORON ${XU4_DEPS}/boron)
add_library(boron STATIC
    ${BORON}/urlan/env.c ${BORON}/urlan/array.c ${BORON}/urlan/binary.c
    ${BORON}/urlan/block.c ${BORON}/urlan/coord.c ${BORON}/urlan/date.c
    ${BORON}/urlan/path.c ${BORON}/urlan/string.c ${BORON}/urlan/context.c
    ${BORON}/urlan/gc.c ${BORON}/urlan/serialize.c ${BORON}/urlan/tokenize.c
    ${BORON}/urlan/vector.c ${BORON}/urlan/parse_block.c
    ${BORON}/urlan/parse_string.c ${BORON}/urlan/hashmap.c
    ${BORON}/support/str.c ${BORON}/support/mem_util.c
    ${BORON}/support/quickSortIndex.c ${BORON}/support/fpconv.c
    ${BORON}/support/well512.c
    ${BORON}/unix/os.c ${BORON}/eval/boron.c ${BORON}/eval/port_file.c
    ${BORON}/eval/wait.c ${BORON}/eval/random.c ${BORON}/eval/port_thread.c)
target_include_directories(boron PRIVATE ${BORON}/include ${BORON}/urlan
                           ${BORON}/eval ${BORON}/support)
target_compile_definitions(boron PRIVATE CONFIG_CHECKSUM CONFIG_COMPRESS=1
    CONFIG_HASHMAP CONFIG_RANDOM CONFIG_STATIC CONFIG_THREAD NDEBUG)
target_compile_options(boron PRIVATE -std=gnu99 -O3 -w)
# xu4 includes <boron/boron.h>.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/inc/boron)
file(READ ${BORON}/include/boron.h BORON_H)
string(REPLACE "\"urlan.h\"" "<boron/urlan.h>" BORON_H "${BORON_H}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/inc/boron/boron.h "${BORON_H}")
configure_file(${BORON}/include/urlan.h ${CMAKE_CURRENT_BINARY_DIR}/inc/boron/urlan.h COPYONLY)
configure_file(${BORON}/include/urlan_atoms.h ${CMAKE_CURRENT_BINARY_DIR}/inc/boron/urlan_atoms.h COPYONLY)

# XU4.
file(READ ${XU4_SRC}/Makefile.common MAKEFILE)
string(REPLACE "$(UI)" "sdl3" MAKEFILE "${MAKEFILE}")
string(REPLACE "$(SOUND)" "sdl3" MAKEFILE "${MAKEFILE}")
string(REGEX MATCHALL "[A-Za-z0-9_/]+\\.(cpp|c)[ \t\n\\\\]" SRCS "${MAKEFILE}")
set(XU4_SRCS ${XU4_SRC}/android.cpp)
foreach(f ${SRCS})
    string(STRIP "${f}" f)
    string(REGEX REPLACE "[ \t\n\\\\]+$" "" f "${f}")
    if(NOT f MATCHES "^util/")
        list(APPEND XU4_SRCS ${XU4_SRC}/${f})
    endif()
endforeach()
list(REMOVE_DUPLICATES XU4_SRCS)

add_library(main SHARED ${XU4_SRCS})
target_include_directories(main PRIVATE ${XU4_SRC} ${XU4_SRC}/support
                           ${CMAKE_CURRENT_BINARY_DIR}/inc)
target_compile_definitions(main PRIVATE USE_BORON CONF_MODULE USE_GLES
                           VERSION="${XU4_VERSION}" NDEBUG)
target_compile_options(main PRIVATE -O3 -Wno-unused-parameter -Wno-format-security
                       $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
target_link_libraries(main PRIVATE SDL3::SDL3-static boron GLESv3 EGL log
                      android z m)
