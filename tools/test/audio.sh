#!/bin/bash
# Record the audio tour (every sound effect & music track) of an xu4 binary.
#
# Usage: tools/test/audio.sh <xu4> <outdir>
#
# The xu4 binary must be a DEBUG build (see build.sh) & XU4_TEST_DATA must be
# set as for run.sh.  PulseAudio (pulseaudio & parec) and an X display are
# needed.  A private PulseAudio server with a null sink is started, so this
# works headless & doesn't play anything aloud.
#
# <outdir> gets tour.txt (what was played when), capture.raw (signed 16-bit
# stereo at 44.1 kHz) and log.txt.  Compare two with audiocmp.py.

TEST_DIR=$(cd $(dirname $0) && pwd)
XU4=$(realpath $1)
OUT=$2
DATA=${XU4_TEST_DATA:?XU4_TEST_DATA must be set}
TEST_HOME=${XU4_TEST_HOME:-/tmp/xu4-test-home}

rm -rf ${OUT} ${TEST_HOME}
mkdir -p ${OUT} ${TEST_HOME}/.config/xu4 ${TEST_HOME}/pulse
OUT=$(realpath ${OUT})

export PULSE_RUNTIME_PATH=${TEST_HOME}/pulse
export PULSE_SINK=xu4test
pulseaudio -n --daemonize=no --exit-idle-time=-1 --disallow-exit \
    --load="module-native-protocol-unix" \
    --load="module-null-sink sink_name=xu4test rate=44100 channels=2" \
    >${OUT}/pulse.log 2>&1 &
PA_PID=$!
for i in $(seq 50); do
    pactl info >/dev/null 2>&1 && break
    sleep 0.1
done

parec -d xu4test.monitor --format=s16le --rate=44100 --channels=2 \
    --latency-msec=20 >${OUT}/capture.raw &
REC_PID=$!
sleep 0.5

(cd ${DATA} && HOME=${TEST_HOME} XU4_TEST_AUDIO=${OUT} \
    SDL_AUDIO_DRIVER=pulseaudio timeout 300 ${XU4} >${OUT}/log.txt 2>&1)
STATUS=$?
sleep 0.5

kill ${REC_PID}
kill ${PA_PID}
wait 2>/dev/null

if [ ${STATUS} -ne 0 ] || [ ! -s ${OUT}/tour.txt ]; then
    echo "Audio tour FAILED (exit ${STATUS})"
    exit 1
fi
echo "Audio tour: $(wc -l <${OUT}/tour.txt) items," \
     "$(( $(stat -c %s ${OUT}/capture.raw) / 176400 )) seconds recorded"
