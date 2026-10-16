#!/bin/bash
# Cross-compile xu4 for 64-bit Windows with MinGW & the SDL3 backend, and
# package it as a zip archive.
#
# Usage: dist/build-windows.sh [version]
#
# Needs: x86_64-w64-mingw32-gcc, cmake, curl, unzip, zip & boron (or the
# modules render.pak, Ultima-IV.mod & U4-Upgrade.mod already built).
# Dependencies are downloaded to build-win/ in the project directory.

set -e

VERSION=${1:-$(git describe --tags --always)}
HOST=x86_64-w64-mingw32
TOP=$(pwd)
WORK=${TOP}/build-win
SYS=${WORK}/usr
DL=${WORK}/dl

SDL_VER=3.2.24
BORON_VER=2.0.8
FAUN_VER=0.2.4
ZLIB_VER=1.3.1
PNG_VER=1.6.50

mkdir -p ${DL} ${SYS}

fetch() {
    [ -f ${DL}/$2 ] || curl -sSfL -o ${DL}/$2 $1/$2
}

fetch https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER} \
      SDL3-devel-${SDL_VER}-mingw.tar.gz
fetch https://xu4.sourceforge.net/download \
      boron-x86_64-w64-mingw32-static-${BORON_VER}.zip
fetch https://codeberg.org/WickedSmoke/faun/releases/download/v${FAUN_VER} \
      faun-${HOST}-static-${FAUN_VER}.zip
fetch https://github.com/madler/zlib/releases/download/v${ZLIB_VER} \
      zlib-${ZLIB_VER}.tar.gz
fetch https://download.sourceforge.net/libpng libpng-${PNG_VER}.tar.gz

# Unpack the SDKs.
if [ ! -f ${SYS}/lib/libpng.a ]; then
    tar xzf ${DL}/SDL3-devel-${SDL_VER}-mingw.tar.gz -C ${WORK}
    cp -r ${WORK}/SDL3-${SDL_VER}/${HOST}/* ${SYS}
    unzip -qo ${DL}/boron-x86_64-w64-mingw32-static-${BORON_VER}.zip -d ${WORK}
    unzip -qo ${DL}/faun-${HOST}-static-${FAUN_VER}.zip -d ${SYS}

    tar xzf ${DL}/zlib-${ZLIB_VER}.tar.gz -C ${WORK}
    make -s -C ${WORK}/zlib-${ZLIB_VER} -f win32/Makefile.gcc \
        PREFIX=${HOST}- libz.a
    cp ${WORK}/zlib-${ZLIB_VER}/libz.a ${SYS}/lib
    cp ${WORK}/zlib-${ZLIB_VER}/{zlib.h,zconf.h} ${SYS}/include

    tar xzf ${DL}/libpng-${PNG_VER}.tar.gz -C ${WORK}
    cmake -S ${WORK}/libpng-${PNG_VER} -B ${WORK}/png-build \
        -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=${HOST}-gcc \
        -DCMAKE_RC_COMPILER=${HOST}-windres \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SYS} \
        -DCMAKE_PREFIX_PATH=${SYS} -DZLIB_ROOT=${SYS} \
        -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF >/dev/null
    cmake --build ${WORK}/png-build -j8 >/dev/null
    cmake --install ${WORK}/png-build >/dev/null
fi

# Build xu4 in a separate tree so that a native build is not disturbed.
SRC=${WORK}/xu4
rm -rf ${SRC}
mkdir -p ${SRC}
git archive HEAD src | tar x -C ${SRC}

FLAGS="-Wall -Wno-unused-parameter -I. -Isupport -I${SYS}/include \
 -I${WORK}/usr/include -DUSE_BORON -DCONF_MODULE -DUSE_GL \
 -DVERSION=\\\"${VERSION}\\\" -O3 -DNDEBUG"
${HOST}-windres -I ${SRC}/src/win32 ${SRC}/src/win32/xu4.rc \
    -o ${SRC}/src/xu4rc.o
make -s -C ${SRC}/src -j8 xu4.exe UI=sdl3 EXEEXT=.exe \
    CC=${HOST}-gcc CXX=${HOST}-g++ \
    CFLAGS="${FLAGS}" CXXFLAGS="${FLAGS}" \
    LDFLAGS="-L${SYS}/lib -static-libgcc -static-libstdc++ -mwindows" \
    LIBS="xu4rc.o -lSDL3 -lfaun ${SYS}/bin/libvorbisfile-3.dll -lole32 -lwinmm \
 -lboron -lws2_32 -lpng -lz -lopengl32 -Wl,-Bstatic -lpthread"

# Package.
PKG=xu4-legacy-${VERSION}-windows
OUT=${WORK}/${PKG}
rm -rf ${OUT}
mkdir -p ${OUT}
cp ${SRC}/src/xu4.exe ${OUT}
cp ${SYS}/bin/SDL3.dll ${SYS}/bin/libvorbisfile-3.dll \
   ${SYS}/bin/libvorbis-0.dll ${SYS}/bin/libogg-0.dll ${OUT}
for f in render.pak Ultima-IV.mod U4-Upgrade.mod; do
    [ -f ${TOP}/$f ] || make -s -C ${TOP} $f
    cp ${TOP}/$f ${OUT}
done
cp ${TOP}/README.md ${OUT}/README.txt
cp ${TOP}/COPYING ${OUT}/COPYING.txt
cp ${TOP}/AUTHORS ${OUT}/AUTHORS.txt
(cd ${WORK} && rm -f ${PKG}.zip && zip -qr ${PKG}.zip ${PKG})
echo ${WORK}/${PKG}.zip
