# Replay regression tests

These tests replay scripted key presses through the game and save
screenshots and save-game files along the way.  Running the same sessions
with two builds (e.g. before and after a change) and comparing the results
shows anything that changed on screen or in the game state.

The replays are deterministic: the random seed is fixed, the game clock is
virtual (one frame per tick, no real sleeping), and audio is off.  A full
run takes well under a minute.  Each session also logs every sound, speech
and music request with its time (`sounds.txt`), so the game must ask for
the same sounds at the same moments.

The audio itself is checked separately by the audio tour, which plays
every sound effect and music track and records the output (see
[Audio](#audio)).

## Requirements

- A DEBUG build of the SDL3 backend, built with `build.sh`.
  (Input replay only exists in DEBUG builds.)
- A data directory (`XU4_TEST_DATA`) holding `ultima4.zip`, `u4upgrad.zip`,
  `render.pak` and the `.mod` files (Ultima-IV.mod, U4-Upgrade.mod).
- An X display (Xvfb works), Python 3, and ImageMagick for `compare.sh`
  pictures.

## Usage

    SDK_DIR=/usr/local tools/test/build.sh . /tmp/xu4-test
    XU4_TEST_DATA=~/data tools/test/run.sh /tmp/xu4-test out/head
    tools/test/compare.sh out/base out/head out/diff

`run.sh` takes optional session files to run only some sessions.
`compare.sh` lists MISSING, EXTRA and DIFFERENT files and, when a diff
directory is given, writes before|after|difference pictures for each changed
screenshot.  It exits non-zero if anything differs.

The `Tests` GitHub workflow does this automatically: it builds the base
commit and the new commit, runs both, and uploads the results and
differences as artifacts.

## Audio

    XU4_TEST_DATA=~/data tools/test/audio.sh /tmp/xu4-test out/head-audio
    tools/test/audiocmp.py out/base-audio out/head-audio out/audio-diff

`audio.sh` starts a private PulseAudio server with a null sink (nothing is
heard), runs the game with `XU4_TEST_AUDIO` set so that it plays each sound
effect and the first 4 seconds of each music track with silence between
them, and records the result.  This takes about two minutes and needs
`pulseaudio`, `parec` and Python with NumPy.

Recordings are never identical, as the sound plays in real time and
different mixers and decoders round differently, so `audiocmp.py` compares
them with tolerances:

- Sound durations (the game waits on some) must match exactly.
- Each item's loudness must be within 1.5 dB, its volume envelope must
  correlate at 0.9 or better, its average spectrum must be at least 0.95
  similar, and a sound's audible length must be within 60 ms.
- Gaps where the recording fell behind (short runs of exact silence) are
  removed, and each half second is lined up separately.

Failing items are written as before and after WAV files to the diff
directory to listen to.  Two recordings of the same build pass with all
scores at 0.99 or above.

## Session scripts

Each `sessions/*.txt` file is converted to a recording by `mkrec.py` (see its
header for the full command list).  For example:

    seed 1234
    include newgame.inc     # Create a character & start the game.
    key ctrl-c m            # Cheat: mixtures.
    wait 30
    key z
    snap                    # Screenshot of the next frame.

- `wait N` waits N frames; `gap N` sets the frames between key presses.
- Cheat commands (`ctrl-c`) work because `run.sh` sets `debug=1`.
- `#! module NAME` runs the session with `-m NAME`.
- The game ends when the recording does.

To add a session, write a script, run it with `run.sh`, and look at the
screenshots (`NNN.ppm`) to check the game reached the intended state.
Waits must be long enough for the slowest screen change, as keys pressed too
early can be dropped.

## Test hooks in the game

In DEBUG builds, when `XU4_TEST_DIR` is set during `--replay`:

- `getTicks()` returns a virtual time advanced by the frame loop.
- The key code `U4_TEST_SNAPSHOT` saves the next frame to `XU4_TEST_DIR`.
- The game quits when the recording ends.
- Sound requests are appended to `sounds.txt` in `XU4_TEST_DIR`.

With `XU4_TEST_AUDIO` set to a directory, the game plays the audio tour
instead, writes `tour.txt` there, and quits.
