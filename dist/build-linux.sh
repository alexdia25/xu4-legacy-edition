#!/bin/bash
# Build xu4 for 64-bit Linux with the SDL3 backend and package it the same
# way as the official XU4 releases: a binary tar.gz (with the setup
# installer), a Debian package and a source archive.
#
# Usage: dist/build-linux.sh [version]
#
# Needs: gcc, cmake, curl, git, dpkg-deb, the X11/Wayland/PulseAudio
# development headers for SDL, libpng, libvorbis & boron (or the modules
# render.pak & the .mod files already built).
# SDL3, Boron & Faun are linked statically; dependencies are downloaded to
# build-linux/ in the project directory.

set -e

VERSION=${1:-$(git describe --tags --always)}
TOP=$(pwd)
WORK=${TOP}/build-linux
SYS=${WORK}/usr
DL=${WORK}/dl

SDL_VER=3.2.24
BORON_VER=2.0.8
FAUN_VER=0.2.4

mkdir -p ${DL} ${SYS}/lib ${SYS}/include

fetch() {
    [ -f ${DL}/$2 ] || curl -sSfL -o ${DL}/$2 $1/$2
}

fetch https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER} \
      SDL3-${SDL_VER}.tar.gz
fetch https://xu4.sourceforge.net/download \
      boron-x86_64-static-${BORON_VER}.tar.bz2
fetch https://codeberg.org/WickedSmoke/faun/releases/download/v${FAUN_VER} \
      faun-x86_64-static-${FAUN_VER}.tar.bz2

if [ ! -f ${SYS}/lib/libSDL3.a ]; then
    tar xjf ${DL}/boron-x86_64-static-${BORON_VER}.tar.bz2 -C ${WORK}
    cp -r ${WORK}/boron/* ${SYS}
    tar xjf ${DL}/faun-x86_64-static-${FAUN_VER}.tar.bz2 -C ${WORK}
    cp -r ${WORK}/faun_sdk-${FAUN_VER}/* ${SYS}

    tar xzf ${DL}/SDL3-${SDL_VER}.tar.gz -C ${WORK}
    cmake -S ${WORK}/SDL3-${SDL_VER} -B ${WORK}/sdl-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SYS} \
        -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF >/dev/null
    cmake --build ${WORK}/sdl-build >/dev/null
    cmake --install ${WORK}/sdl-build >/dev/null
fi

# Build xu4 in a separate tree so that a development build is not disturbed.
SRC=${WORK}/xu4
rm -rf ${SRC}
mkdir -p ${SRC}
git archive HEAD src | tar x -C ${SRC}

FLAGS="-Wall -Wno-unused-parameter -I. -Isupport -I${SYS}/include \
 -DUSE_BORON -DCONF_MODULE -DVERSION=\\\"${VERSION}\\\" -O3 -DNDEBUG"
make -s -C ${SRC}/src -j8 xu4 UI=sdl3 \
    CFLAGS="${FLAGS}" CXXFLAGS="${FLAGS}" \
    LDFLAGS="-L${SYS}/lib -no-pie" \
    LIBS="-Wl,-Bstatic -lSDL3 -lfaun -lboron -Wl,-Bdynamic \
 -lpulse-simple -lpulse -lvorbisfile -lGL -lpng -lz -lpthread -ldl -lm"
strip ${SRC}/src/xu4

MODULES="render.pak Ultima-IV.mod U4-Upgrade.mod Apple-II.mod Apple-II-Music.mod C64.mod C64-Music.mod C64-Remastered.mod"
for f in ${MODULES}; do
    [ -f ${TOP}/$f ] || make -s -C ${TOP} $f
done

# Binary archive, laid out like the official one.
PKG=xu4-legacy-${VERSION}
OUT=${WORK}/${PKG}
rm -rf ${OUT}
mkdir -p ${OUT}/desktop
cp ${SRC}/src/xu4 ${OUT}
(cd ${TOP} && cp ${MODULES} COPYING dist/setup ${OUT})
cp ${TOP}/README.md ${OUT}/README
cp ${TOP}/icons/shield.png ${OUT}/desktop/xu4.png
cp ${TOP}/dist/xu4.desktop ${OUT}/desktop
tar czf ${WORK}/${PKG}-linux.tar.gz -C ${WORK} ${PKG}

# Debian package.
DEB=${WORK}/deb
rm -rf ${DEB}
mkdir -p ${DEB}/DEBIAN ${DEB}/usr/bin ${DEB}/usr/share/xu4 \
         ${DEB}/usr/share/applications \
         ${DEB}/usr/share/icons/hicolor/48x48/apps \
         ${DEB}/usr/share/doc/xu4-legacy
install -m 755 ${SRC}/src/xu4 ${DEB}/usr/bin
(cd ${TOP} && install -m 644 ${MODULES} ${DEB}/usr/share/xu4)
install -m 644 ${TOP}/icons/shield.png \
        ${DEB}/usr/share/icons/hicolor/48x48/apps/xu4.png
install -m 644 ${TOP}/dist/xu4.desktop ${DEB}/usr/share/applications
install -m 644 ${TOP}/README.md ${TOP}/COPYING ${DEB}/usr/share/doc/xu4-legacy
DEBVER=$(echo ${VERSION} | sed 's/^[^0-9]*//; s/-/./g')
cat > ${DEB}/DEBIAN/control <<EOF
Package: xu4-legacy
Version: ${DEBVER:-0}
Architecture: amd64
Maintainer: XU4 Legacy Edition <noreply@users.noreply.github.com>
Depends: libc6, libstdc++6, libgl1, libpng16-16t64 | libpng16-16, zlib1g, libpulse0, libvorbisfile3
Conflicts: xu4
Replaces: xu4
Section: games
Priority: optional
Homepage: https://github.com/alexdia25/xu4-legacy-edition
Description: Ultima IV recreated, with gamepad and console style menus
 XU4 Legacy Edition is a fork of XU4, the remake of Ultima IV that runs
 the original DOS game data.  It adds gamepad support, console (Sega
 Master System) style menus and the game documents in the game.
 .
 The Ultima IV for DOS data (ultima4.zip) must be installed separately.
EOF
dpkg-deb --root-owner-group --build ${DEB} \
    ${WORK}/xu4-legacy_${DEBVER:-0}_amd64.deb >/dev/null

# Source archive (with the X11 GLV files, as dist/tar-release does).
SDIR=${WORK}/src-archive
rm -rf ${SDIR}
mkdir -p ${SDIR}/${PKG}
git archive HEAD | tar x -C ${SDIR}/${PKG}
if [ -d ${TOP}/src/glv/x11 ]; then
    mkdir -p ${SDIR}/${PKG}/src/glv
    cp -r ${TOP}/src/glv/LICENSE ${TOP}/src/glv/x11 ${SDIR}/${PKG}/src/glv
fi
rm -rf ${SDIR}/${PKG}/.git*
tar czf ${WORK}/${PKG}.tar.gz -C ${SDIR} ${PKG}

ls ${WORK}/${PKG}-linux.tar.gz ${WORK}/xu4-legacy_*.deb ${WORK}/${PKG}.tar.gz
