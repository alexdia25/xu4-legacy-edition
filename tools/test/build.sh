#!/bin/bash
# Build a DEBUG xu4 binary (SDL3 backend) for the replay tests.
#
# Usage: tools/test/build.sh <project-dir> <out-binary>
#
# Works on trees before & after the SDL3-only change: when
# src/sound_faun.cpp exists the Faun, Vorbis, PulseAudio & libpng libraries
# are linked as well.  SDK_DIR (default /usr/local) holds the SDL3 & Boron
# (& Faun) headers and static libraries.

set -e
TOP=$(realpath $1)
OUT=$(realpath -m $2)
SDK=${SDK_DIR:-/usr/local}
WORK=$(mktemp -d)
trap "rm -rf ${WORK}" EXIT

cp -r ${TOP}/src ${WORK}/src
rm -f ${WORK}/src/*.o ${WORK}/src/*/*.o

# The same VERSION for every build, as it is shown on the About screen.
FLAGS="-Wall -Wno-unused-parameter -I. -Isupport -I${SDK}/include \
 -DUSE_BORON -DCONF_MODULE -DVERSION=\\\"test\\\" -O1 -g -DDEBUG"
LIBS="-Wl,-Bstatic -lSDL3 -lboron"
if [ -f ${WORK}/src/sound_faun.cpp ]; then
    LIBS="${LIBS} -lfaun -Wl,-Bdynamic -lpulse-simple -lpulse -lvorbisfile -lGL -lpng"
else
    LIBS="${LIBS} -Wl,-Bdynamic"
fi
make -s -C ${WORK}/src -j$(nproc) xu4 UI=sdl3 SOUND=$( \
    [ -f ${WORK}/src/sound_faun.cpp ] && echo faun || echo sdl3) \
    CFLAGS="${FLAGS}" CXXFLAGS="${FLAGS}" LDFLAGS="-L${SDK}/lib -no-pie" \
    LIBS="${LIBS} -lz -lpthread -ldl -lm" 2>&1 | grep -E " error:|undefined reference" || true
mkdir -p $(dirname ${OUT})
cp ${WORK}/src/xu4 ${OUT}
echo ${OUT}
