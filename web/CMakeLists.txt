# The web (Emscripten) build: xu4.html/.js/.wasm/.data, run by
# dist/build-web.sh with emcmake.  XU4_DEPS holds the SDL, boron & zlib sources and
# XU4_ASSETS the modules, packed into the page's /data folder.

cmake_minimum_required(VERSION 3.22)
project(xu4 C CXX)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${XU4_DEPS}/SDL SDL)

# zlib, from source (dist/build-web.sh downloads it).
set(Z ${XU4_DEPS}/zlib)
add_library(zlib STATIC ${Z}/adler32.c ${Z}/compress.c ${Z}/crc32.c
    ${Z}/deflate.c ${Z}/infback.c ${Z}/inffast.c ${Z}/inflate.c
    ${Z}/inftrees.c ${Z}/trees.c ${Z}/uncompr.c ${Z}/zutil.c)
target_compile_options(zlib PRIVATE -O3 -w)
target_include_directories(zlib PUBLIC ${Z})
# The same z_off_t (off_t) in C & C++, which defines _LARGEFILE64_SOURCE.
target_compile_definitions(zlib PUBLIC HAVE_UNISTD_H)

# No threads on the web.
set(BORON_DEFS CONFIG_CHECKSUM CONFIG_COMPRESS=1 CONFIG_HASHMAP CONFIG_RANDOM
    CONFIG_STATIC)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/xu4.cmake)
target_link_libraries(boron PRIVATE zlib)

add_executable(xu4 ${XU4_SRCS} ${XU4_SRC}/web.cpp)
xu4_target(xu4)
set_target_properties(xu4 PROPERTIES SUFFIX ".html")
target_link_libraries(xu4 PRIVATE SDL3::SDL3-static boron zlib)
target_link_options(xu4 PRIVATE
    # The game's loops wait for input & frames; ASYNCIFY lets them return to
    # the browser while they sleep.
    -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 -sSTACK_SIZE=2097152
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
    -sENVIRONMENT=web -lidbfs.js
    -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,addRunDependency,removeRunDependency,callMain
    --preload-file ${XU4_ASSETS}@/data
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)
