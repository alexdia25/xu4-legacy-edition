#!/bin/bash
# Build xu4 for Android as an APK:
#   build-android/xu4-legacy-<version>-android.apk
#
# Usage: dist/build-android.sh [version]
#
# Needs: the Android SDK (ANDROID_HOME) with platform 35 & NDK 27.2, a JDK
# (17 or later), gradle, curl, git & boron (or the modules render.pak & the
# .mod files already built).  SDL3 & Boron sources are downloaded to
# build-android/deps.  The APK is signed with the key in XU4_KEYSTORE (see
# android/README.md), or else with the local debug key.

set -e

VERSION=${1:-$(git describe --tags --always)}
TOP=$(pwd)
WORK=${TOP}/build-android
DL=${WORK}/dl
DEPS=${WORK}/deps
ASSETS=${WORK}/assets

SDL_VER=3.4.18
BORON_VER=2.0.8

mkdir -p ${DL} ${DEPS} ${ASSETS}

if [ ! -d ${DEPS}/SDL ]; then
    [ -f ${DL}/SDL3-${SDL_VER}.tar.gz ] || curl -sSfL -o ${DL}/SDL3-${SDL_VER}.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VER}/SDL3-${SDL_VER}.tar.gz
    tar xzf ${DL}/SDL3-${SDL_VER}.tar.gz -C ${DEPS}
    mv ${DEPS}/SDL3-${SDL_VER} ${DEPS}/SDL
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

# A version code that grows with every commit, so a newer APK installs over
# an older one.
CODE=$(git rev-list --count HEAD)

(cd ${TOP}/android && gradle --no-daemon -q assembleRelease \
    -Pxu4.version="${VERSION}" -Pxu4.versionCode=${CODE} \
    -Pxu4.deps=${DEPS} -Pxu4.assets=${ASSETS})

cp ${TOP}/android/app/build/outputs/apk/release/app-release.apk \
   ${WORK}/xu4-legacy-${VERSION}-android.apk
ls ${WORK}/xu4-legacy-${VERSION}-android.apk
