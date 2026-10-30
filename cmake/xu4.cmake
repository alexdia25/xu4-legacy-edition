# Shared by the Android (android/app/jni) and web (web/) CMake builds:
# a static Boron library target "boron" and the list XU4_SRCS of the xu4
# sources, read from src/Makefile.common so that every build compiles the
# same files.  Needs XU4_DEPS, the directory holding the boron sources, and
# BORON_DEFS, Boron's configuration defines.

set(XU4_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

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
    ${BORON}/eval/wait.c ${BORON}/eval/random.c)
if("CONFIG_THREAD" IN_LIST BORON_DEFS)
    target_sources(boron PRIVATE ${BORON}/eval/port_thread.c)
endif()
target_include_directories(boron PRIVATE ${BORON}/include ${BORON}/urlan
                           ${BORON}/eval ${BORON}/support)
target_compile_definitions(boron PRIVATE ${BORON_DEFS} NDEBUG)
target_compile_options(boron PRIVATE -std=gnu99 -O3 -w)

# xu4 includes <boron/boron.h>.
set(XU4_INC ${CMAKE_CURRENT_BINARY_DIR}/inc)
file(MAKE_DIRECTORY ${XU4_INC}/boron)
file(READ ${BORON}/include/boron.h BORON_H)
string(REPLACE "\"urlan.h\"" "<boron/urlan.h>" BORON_H "${BORON_H}")
file(WRITE ${XU4_INC}/boron/boron.h "${BORON_H}")
configure_file(${BORON}/include/urlan.h ${XU4_INC}/boron/urlan.h COPYONLY)
configure_file(${BORON}/include/urlan_atoms.h ${XU4_INC}/boron/urlan_atoms.h COPYONLY)

file(READ ${XU4_SRC}/Makefile.common MAKEFILE)
string(REPLACE "$(UI)" "sdl3" MAKEFILE "${MAKEFILE}")
string(REPLACE "$(SOUND)" "sdl3" MAKEFILE "${MAKEFILE}")
string(REGEX MATCHALL "[A-Za-z0-9_/]+\\.(cpp|c)[ \t\n\\\\]" SRCS "${MAKEFILE}")
set(XU4_SRCS)
foreach(f ${SRCS})
    string(REGEX REPLACE "[ \t\n\\\\]+$" "" f "${f}")
    if(NOT f MATCHES "^util/")
        list(APPEND XU4_SRCS ${XU4_SRC}/${f})
    endif()
endforeach()
list(REMOVE_DUPLICATES XU4_SRCS)

# Compile options for the xu4 target.
function(xu4_target target)
    target_include_directories(${target} PRIVATE ${XU4_SRC} ${XU4_SRC}/support
                               ${XU4_INC})
    target_compile_definitions(${target} PRIVATE USE_BORON CONF_MODULE USE_GLES
                               VERSION="${XU4_VERSION}" NDEBUG)
    target_compile_options(${target} PRIVATE -O3 -Wno-unused-parameter
                           -Wno-format-security
                           $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
endfunction()
