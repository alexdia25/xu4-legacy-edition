#!/bin/bash
# Replay the test sessions with an xu4 binary & collect the results.
#
# Usage: tools/test/run.sh <xu4> <outdir> [session.txt ...]
#
# The xu4 binary must be a DEBUG build of the SDL3 backend (see build.sh).
# XU4_TEST_DATA must name a directory holding ultima4.zip, u4upgrad.zip,
# render.pak & the .mod files.  An X display is needed (e.g. Xvfb).
#
# For each session, <outdir>/<session>/ gets the screenshots (NNN.ppm), the
# saved game files, and the console log (log.txt, not compared).

TEST_DIR=$(cd $(dirname $0) && pwd)
XU4=$(realpath $1)
OUT=$2
shift 2
SESSIONS=${@:-${TEST_DIR}/sessions/*.txt}
DATA=${XU4_TEST_DATA:?XU4_TEST_DATA must be set}
# The home directory path appears on some screens, so it must not change
# between runs.
TEST_HOME=${XU4_TEST_HOME:-/tmp/xu4-test-home}

mkdir -p ${OUT}
OUT=$(realpath ${OUT})
FAILED=0

for SESSION in ${SESSIONS}; do
    NAME=$(basename ${SESSION} .txt)
    DEST=${OUT}/${NAME}
    rm -rf ${DEST} ${TEST_HOME}
    mkdir -p ${DEST} ${TEST_HOME}/.config/xu4

    # Settings: enable the debug (cheat) commands.
    # The Legacy Edition conveniences are off so that play is as it was.
    printf "debug=1\nbumpToAct=0\nfoldMoves=0\nfreeCancel=0\nshopCompare=0\nspellInfo=0\nhealShortcut=0\ndungeonMap=Off\n" > ${TEST_HOME}/.config/xu4/xu4rc

    ARGS=""
    MODULE=$(sed -n 's/^#! *module *//p' ${SESSION})
    [ -n "${MODULE}" ] && ARGS="-m ${MODULE}"

    python3 ${TEST_DIR}/mkrec.py ${SESSION} ${DEST}/session.rec || exit 2

    # Audio is off (-q) so that sound durations, which some waits depend
    # on, are zero for every sound backend.
    (cd ${DATA} && HOME=${TEST_HOME} XU4_TEST_DIR=${DEST} \
        SDL_AUDIO_DRIVER=dummy timeout 300 \
        ${XU4} -q ${ARGS} --replay ${DEST}/session.rec >${DEST}/log.txt 2>&1)
    STATUS=$?
    rm ${DEST}/session.rec

    cp -r ${TEST_HOME}/.config/xu4 ${DEST}/save
    rm -f ${DEST}/save/xu4rc

    SNAPS=$(ls ${DEST}/*.ppm 2>/dev/null | wc -l)
    if [ ${STATUS} -ne 0 ]; then
        echo "${NAME}: FAILED (exit ${STATUS}, ${SNAPS} screenshots)"
        tail -5 ${DEST}/log.txt
        FAILED=1
    else
        echo "${NAME}: ${SNAPS} screenshots"
    fi
done
exit ${FAILED}
