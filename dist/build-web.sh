#!/bin/bash
# Build xu4 for the web with Emscripten:
#   build-web/site/ (index.html, xu4.js, xu4.wasm, xu4.data, the PWA files)
# which any static web server can serve (GitHub Pages serves it under /play/).
#
# Usage: dist/build-web.sh [version]
#
# Needs: the Emscripten SDK (emcmake in the PATH), cmake, curl, git & boron
# (or the modules render.pak & the .mod files already built).  SDL3 & Boron
# & zlib sources are downloaded to build-web/deps.

set -e

VERSION=${1:-$(git describe --tags --always)}
TOP=$(pwd)
WORK=${TOP}/build-web
DL=${WORK}/dl
DEPS=${WORK}/deps
ASSETS=${WORK}/assets
SITE=${WORK}/site

SDL_VER=3.4.18
BORON_VER=2.0.8
ZLIB_VER=1.3.1

mkdir -p ${DL} ${DEPS} ${ASSETS}

if [ ! -d ${DEPS}/SDL ]; then
    [ -f ${DL}/SDL3-${SDL_VER}.tar.gz ] || curl -sSfL -o ${DL}/SDL3-${SDL_VER}.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER}/SDL3-${SDL_VER}.tar.gz
    tar xzf ${DL}/SDL3-${SDL_VER}.tar.gz -C ${DEPS}
    mv ${DEPS}/SDL3-${SDL_VER} ${DEPS}/SDL
fi
if [ ! -d ${DEPS}/zlib ]; then
    [ -f ${DL}/zlib-${ZLIB_VER}.tar.gz ] || curl -sSfL -o ${DL}/zlib-${ZLIB_VER}.tar.gz \
        https://github.com/madler/zlib/releases/download/v${ZLIB_VER}/zlib-${ZLIB_VER}.tar.gz
    tar xzf ${DL}/zlib-${ZLIB_VER}.tar.gz -C ${DEPS}
    mv ${DEPS}/zlib-${ZLIB_VER} ${DEPS}/zlib
fi
if [ ! -d ${DEPS}/boron ]; then
    git clone -q --depth 1 -b v${BORON_VER} https://github.com/0branch/boron ${DEPS}/boron
fi

MODULES="render.pak Ultima-IV.mod U4-Upgrade.mod Apple-II.mod Apple-II-Music.mod C64.mod C64-Music.mod C64-Remastered.mod SMS.mod SMS-Music.mod"
for f in ${MODULES}; do
    # Rebuilt when out of date; kept as is if boron is not installed.
    make -s -C ${TOP} $f || [ -f ${TOP}/$f ]
    cp ${TOP}/$f ${ASSETS}
done

emcmake cmake -S ${TOP}/web -B ${WORK}/build -DCMAKE_BUILD_TYPE=Release \
    -DXU4_DEPS=${DEPS} -DXU4_ASSETS=${ASSETS} -DXU4_VERSION="${VERSION}" >/dev/null
cmake --build ${WORK}/build -j8

rm -rf ${SITE}
mkdir -p ${SITE}
cp ${WORK}/build/xu4.html ${SITE}/index.html
cp ${WORK}/build/xu4.js ${WORK}/build/xu4.wasm ${WORK}/build/xu4.data ${SITE}
cp ${TOP}/web/manifest.webmanifest ${SITE}
sed "s/@VERSION@/${VERSION}-$(date +%s)/" ${TOP}/web/sw.js > ${SITE}/sw.js
cp ${TOP}/web/icon-192.png ${TOP}/web/icon-512.png ${SITE}
ls -la ${SITE}
