#
# Makefile for Linux & macOS
#

-include ../make.config

UI=sdl3
SOUND=sdl3
GPU ?= scale

UILIBS=-lSDL3
UIFLAGS=

ifeq ($(GPU),all)
	UIFLAGS+=-DGPU_RENDER
endif

# Uncomment this for Raspberry Pi 4.
#UIFLAGS+=-DUSE_GLES

#ifeq ($(CONF),boron)
	UIFLAGS+=-DUSE_BORON -DCONF_MODULE
	UILIBS+=-lboron -lpthread
#endif

CXXFLAGS=-Wall -I. -Isupport $(UIFLAGS) -DVERSION=\"$(VERSION)\"
#CXXFLAGS+=-rdynamic -DHAVE_BACKTRACE=1 -DHAVE_VARIADIC_MACROS=1

# Choose one of these for debug/release mode.
#CXXFLAGS+=-g -DDEBUG
CXXFLAGS+=-O3 -DNDEBUG

CFLAGS=$(CXXFLAGS)

# OpenGL functions are obtained through SDL, except on macOS.
LIBS=$(UILIBS) -lz
ifeq ($(shell uname),Darwin)
LIBS+=-framework OpenGL
endif

ifeq ($(STATIC_GCC_LIBS),true)
    LDFLAGS+=-L. -static-libgcc
endif

include Makefile.common

# use GCC's automatic dependency generation
.depends::
	rm -f .depends
	$(CC) $(CFLAGS) -MM $(CSRCS) >> .depends
	$(CXX) $(CXXFLAGS) -MM $(CXXSRCS) >> .depends

# a target to build without dependencies on libstdc++, libgcc_s
all.static_gcc_libs:
	ln -s `$(CXX) -print-file-name=libstdc++.a` .
	$(MAKE) STATIC_GCC_LIBS=true
	rm -f libstdc++.a

-include .depends

