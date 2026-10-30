#!/bin/bash
# Build xu4 for macOS as a universal (Apple Silicon & Intel) app with the
# SDL3 backend, and package it as a disk image: build-mac/xu4-legacy-<version>-macos.dmg
#
# Usage: dist/build-macos.sh [version]
#
# Needs: Xcode command line tools, cmake, curl & git.  SDL3 & Boron are built
# from source and linked statically; dependencies go to build-mac/ in the
# project directory.  The app is ad-hoc signed only (no Apple developer ID),
# so macOS asks the player to allow it once (see README.md).

set -e

VERSION=${1:-$(git describe --tags --always)}
TOP=$(pwd)
WORK=${TOP}/build-mac
SYS=${WORK}/usr
DL=${WORK}/dl

SDL_VER=3.4.18
BORON_VER=2.0.8
export MACOSX_DEPLOYMENT_TARGET=11.0
ARCHS="-arch arm64 -arch x86_64"

mkdir -p ${DL} ${SYS}/lib ${SYS}/include ${SYS}/bin

fetch() {
    [ -f ${DL}/$2 ] || curl -sSfL -o ${DL}/$2 $1/$2
}

if [ ! -f ${SYS}/lib/libSDL3.a ]; then
    fetch https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER} \
          SDL3-${SDL_VER}.tar.gz
    tar xzf ${DL}/SDL3-${SDL_VER}.tar.gz -C ${WORK}
    cmake -S ${WORK}/SDL3-${SDL_VER} -B ${WORK}/sdl-build \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SYS} \
        -DCMAKE_INSTALL_LIBDIR=lib \
        "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF >/dev/null
    cmake --build ${WORK}/sdl-build -j8 >/dev/null
    cmake --install ${WORK}/sdl-build >/dev/null
fi

if [ ! -f ${SYS}/lib/libboron.a ]; then
    rm -rf ${WORK}/boron
    git clone -q --depth 1 -b v${BORON_VER} https://github.com/0branch/boron ${WORK}/boron
    # The Boron Makefile calls cc itself, so the architectures go in CFLAGS.
    (cd ${WORK}/boron && ./configure --static --thread --no-socket >/dev/null &&
     make -s -j8 CFLAGS="-pipe -Wall -Iinclude -Iurlan -Ieval -Isupport -O3 -DNDEBUG -std=c99 ${ARCHS} -mmacosx-version-min=${MACOSX_DEPLOYMENT_TARGET}")
    make -s -C ${WORK}/boron DESTDIR=${SYS} install-dev
    cp ${WORK}/boron/boron ${SYS}/bin
fi
export PATH=${SYS}/bin:${PATH}

# SDL's static library needs the system frameworks listed in sdl3.pc.
SDL_LIBS=$(sed -n 's/^Libs.private: *//p' ${SYS}/lib/pkgconfig/sdl3.pc)

# Build xu4 in a separate tree so that a development build is not disturbed.
SRC=${WORK}/xu4
rm -rf ${SRC}
mkdir -p ${SRC}
git archive HEAD src | tar x -C ${SRC}

FLAGS="-Wall -Wno-unused-parameter -I. -Isupport -I${SYS}/include \
 -DUSE_BORON -DCONF_MODULE \
 -DVERSION=\\\"${VERSION}\\\" -O3 -DNDEBUG"
make -s -C ${SRC}/src -j8 xu4 UI=sdl3 \
    CC="clang ${ARCHS}" CXX="clang++ ${ARCHS}" \
    CFLAGS="${FLAGS}" CXXFLAGS="${FLAGS}" \
    LDFLAGS="-L${SYS}/lib" \
    LIBS="-lSDL3 -lboron -lz -lpthread -lm ${SDL_LIBS} -framework OpenGL"
strip -x ${SRC}/src/xu4
lipo -info ${SRC}/src/xu4

MODULES="render.pak Ultima-IV.mod U4-Upgrade.mod Apple-II.mod Apple-II-Music.mod C64.mod C64-Music.mod C64-Remastered.mod SMS.mod SMS-Music.mod"
for f in ${MODULES}; do
    # Rebuilt when out of date; kept as is if boron cannot run.
    make -s -C ${TOP} $f || [ -f ${TOP}/$f ]
done

# The app bundle.  xu4 finds render.pak & the modules in Resources, and the
# game data & saves in ~/Library/Application Support/xu4.
APP=${WORK}/dmg/XU4\ Legacy.app
rm -rf ${WORK}/dmg
mkdir -p "${APP}/Contents/MacOS" "${APP}/Contents/Resources"
cp ${SRC}/src/xu4 "${APP}/Contents/MacOS/xu4"
(cd ${TOP} && cp ${MODULES} "${APP}/Contents/Resources")
cp ${TOP}/icons/xu4.icns "${APP}/Contents/Resources"
PLVER=$(echo ${VERSION} | sed 's/^[^0-9]*//; s/-.*//')
cat > "${APP}/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>xu4</string>
	<key>CFBundleIconFile</key>
	<string>xu4.icns</string>
	<key>CFBundleIdentifier</key>
	<string>io.github.alexdia25.xu4_legacy_edition</string>
	<key>CFBundleName</key>
	<string>XU4 Legacy</string>
	<key>CFBundleDisplayName</key>
	<string>XU4 Legacy Edition</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>${PLVER:-0}</string>
	<key>CFBundleVersion</key>
	<string>${PLVER:-0}</string>
	<key>LSApplicationCategoryType</key>
	<string>public.app-category.role-playing-games</string>
	<key>LSMinimumSystemVersion</key>
	<string>${MACOSX_DEPLOYMENT_TARGET}</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSPrincipalClass</key>
	<string>NSApplication</string>
</dict>
</plist>
EOF
codesign --force --deep --sign - "${APP}"

cp ${TOP}/COPYING ${WORK}/dmg/COPYING.txt
cp ${TOP}/README.md ${WORK}/dmg/README.md
ln -s /Applications ${WORK}/dmg/Applications
DMG=${WORK}/xu4-legacy-${VERSION}-macos.dmg
rm -f ${DMG}
hdiutil create -quiet -fs HFS+ -volname "XU4 Legacy ${VERSION}" \
    -srcfolder ${WORK}/dmg -format UDZO ${DMG}

ls ${DMG}
