#!/usr/bin/env python3
"""
Compare two audio tour recordings made by audio.sh.

Usage: audiocmp.py <before-dir> <after-dir> [diff-dir]

Sound effect durations (which the game waits on) must match exactly.  Each
sound & music track is cut from the recordings and the two are compared
with tolerances, since different mixers & decoders never give identical
samples:

    level   Difference in loudness (RMS), dB.
    shape   Correlation of the volume envelopes (10 ms steps), 0-1.
    tone    Similarity of the average spectra, 0-1.
    length  Difference in audible length, ms (sounds only).

Short gaps of exact silence, where the player fell behind while recording,
are removed first.

If diff-dir is given, WAV files of both versions of every failing item are
written there to listen to.  Exits with status 1 if anything fails.
"""

import os
import sys
import wave

import numpy as np

RATE = 44100
STEP = RATE // 100              # Envelope step: 10 ms.
SILENCE = 10 ** (-50 / 20)      # -50 dBFS
SEARCH = 0.4                    # Seconds to search after the expected start.
QUIET_STEPS = 25                # Silence before a sound starts: 250 ms.
DROPOUT_MAX = RATE // 5         # Longest recording gap to remove: 200 ms.

MAX_LEVEL_DB = 1.5
MIN_SHAPE = 0.9
MIN_TONE = 0.95
MAX_LENGTH_MS = 60

SOUND_NAMES = """title-fade walk-normal walk-slowed walk-combat blocked error
pc-attack pc-struck npc-attack npc-struck acid sleep poison-effect
poison-damage evade flee item-stolen lb-heal level-up elevate moongate cannon
party-struck rumble premagic magic whirlpool storm gate-open stone-falling
wind-gust ui-click ui-tick fizzle ignite""".split()
MUSIC_NAMES = """none outside towns shrines shopping rulebrit fanfare dungeon
combat castles""".split()


def load(path):
    items = []
    for line in open(os.path.join(path, 'tour.txt')):
        kind, num, start, length = line.split()
        items.append((kind, int(num), int(start) / 1000, int(length) / 1000))
    raw = np.fromfile(os.path.join(path, 'capture.raw'), dtype='<i2')
    pcm = raw[:len(raw) // 2 * 2].reshape(-1, 2).astype(np.float32) / 32768
    return items, pcm


def name(kind, num):
    names = SOUND_NAMES if kind == 's' else MUSIC_NAMES
    label = names[num] if num < len(names) else str(num)
    return ('sound ' if kind == 's' else 'music ') + label


def envelope(pcm):
    mono = pcm.mean(axis=1)
    n = len(mono) // STEP
    return np.sqrt((mono[:n * STEP].reshape(n, STEP) ** 2).mean(axis=1))


def starts(pcm):
    """Return the sample positions where sound begins after a silence."""
    loud = envelope(pcm) > SILENCE
    found = []
    quiet = QUIET_STEPS
    for i, on in enumerate(loud):
        if on:
            if quiet >= QUIET_STEPS:
                found.append(i * STEP)
            quiet = 0
        else:
            quiet += 1
    return np.array(found)


def remove_dropouts(pcm):
    """
    Remove short runs of exact silence in the middle of a sound.  These are
    where the player fell behind while recording (a real recording is never
    exactly zero while sound is playing).
    """
    zero = ~pcm.any(axis=1)
    edges = np.flatnonzero(np.diff(zero.astype(np.int8)))
    keep = np.ones(len(pcm), dtype=bool)
    # Runs of zeros start after a +1 edge and end at the next -1 edge.
    for begin, end in zip(edges[:-1], edges[1:]):
        if zero[begin + 1] and 0 < end - begin <= DROPOUT_MAX:
            keep[begin + 1:end + 1] = False
    return pcm[keep]


def segments(items, pcm):
    """Cut out each played item, aligned on where its sound begins."""
    played = [it for it in items if it[3] > 0]
    begins = starts(pcm)
    if not len(begins) or not played:
        return {}
    expect = np.array([int(it[2] * RATE) for it in played])
    early, late = int(0.1 * RATE), int(SEARCH * RATE)

    def match(offset):
        found = []
        for e in expect + offset:
            near = begins[(begins >= e - early) & (begins <= e + late)]
            found.append(near[0] if len(near) else None)
        return found

    # The recording begins at an unknown time: try lining up each of the
    # first sounds heard with each of the first items, & keep the offset
    # that matches the most items.
    best = None
    for b in begins[:8]:
        for e in expect[:8]:
            found = match(b - e)
            score = sum(f is not None for f in found)
            if best is None or score > best[0]:
                best = (score, found)

    segs = {}
    for (kind, num, start, length), at in zip(played, best[1]):
        if at is None:
            segs[(kind, num)] = None
            continue
        size = int(length * RATE)
        if kind == 's':
            size += int(0.3 * RATE)     # Allow for a longer tail.
        seg = remove_dropouts(pcm[at:at + size + int(0.5 * RATE)])
        segs[(kind, num)] = seg[:size]
    return segs


def audible_ms(seg):
    env = envelope(seg)
    loud = np.nonzero(env > SILENCE)[0]
    return (loud[-1] + 1) * 10 if len(loud) else 0


def spectrum(seg):
    mono = seg.mean(axis=1)
    size = 4096
    if len(mono) < size:
        mono = np.pad(mono, (0, size - len(mono)))
    n = len(mono) // size
    frames = mono[:n * size].reshape(n, size) * np.hanning(size)
    mag = np.abs(np.fft.rfft(frames, axis=1)).mean(axis=0)
    # 48 bands spaced evenly in log frequency from 40 Hz.
    freqs = np.fft.rfftfreq(size, 1 / RATE)
    edges = np.geomspace(40, RATE / 2, 49)
    bands = np.array([mag[(freqs >= lo) & (freqs < hi)].sum()
                      for lo, hi in zip(edges[:-1], edges[1:])])
    return np.log1p(bands * 100)


def similarity(a, b):
    na, nb = np.linalg.norm(a), np.linalg.norm(b)
    if na == 0 or nb == 0:
        return 1.0 if na == nb else 0.0
    return float(np.dot(a, b) / (na * nb))


def align(a, b):
    """Return how many samples b lags a (within half a second)."""
    n = min(len(a), len(b), 2 * RATE)
    x, y = a[:n].mean(axis=1), b[:n].mean(axis=1)
    size = 1 << (2 * n - 1).bit_length()
    corr = np.fft.irfft(np.fft.rfft(y, size) * np.conj(np.fft.rfft(x, size)),
                        size)
    limit = RATE // 2
    lags = np.concatenate((corr[:limit + 1], corr[-limit:]))
    lag = int(np.argmax(lags))
    return lag if lag <= limit else lag - len(lags)


def shape_match(a, b):
    """
    Compare the volume envelopes of two recordings in half second pieces,
    each lined up on its own (within 50 ms) as real time recordings can
    skip or stall a little.  Return the median of the piece correlations,
    so that one glitch doesn't fail a whole track.
    """
    piece, slack = RATE // 2, RATE // 20
    x_all, y_all = a.mean(axis=1), b.mean(axis=1)
    scores = []
    for i in range(0, len(x_all), piece):
        x = x_all[i:i + piece]
        if len(x) < STEP * 5:
            break
        lo = max(0, i - slack)
        y = y_all[lo:i + len(x) + slack]
        if len(y) < len(x):
            break
        if np.abs(x).max() > SILENCE and np.abs(y).max() > SILENCE:
            corr = np.correlate(y, x, 'valid')
            y = y[int(np.argmax(corr)):][:len(x)]
        else:
            y = y[i - lo:][:len(x)]
        ex, ey = envelope(x[:, None]), envelope(y[:, None])
        if ex.std() > 0 and ey.std() > 0:
            scores.append(float(np.corrcoef(ex, ey)[0, 1]))
        else:
            scores.append(1.0 if np.allclose(ex, ey, atol=SILENCE) else 0.0)
    return float(np.median(scores)) if scores else 1.0


def compare(a, b):
    lag = align(a, b)
    if lag > 0:
        b = b[lag:]
    elif lag < 0:
        a = a[-lag:]
    n = min(len(a), len(b))
    shape = shape_match(a[:n], b[:n])
    rms_a = np.sqrt((a[:n] ** 2).mean()) + 1e-9
    rms_b = np.sqrt((b[:n] ** 2).mean()) + 1e-9
    return {
        'level': 20 * np.log10(rms_b / rms_a),
        'shape': shape,
        'tone': similarity(spectrum(a[:n]), spectrum(b[:n])),
        'length': audible_ms(b) - audible_ms(a),
    }


def write_wav(path, seg):
    with wave.open(path, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(seg, -1, 1) * 32767).astype('<i2').tobytes())


def main(before, after, diffdir=None):
    items_a, pcm_a = load(before)
    items_b, pcm_b = load(after)
    failed = 0

    # Durations.
    dur_a = {(k, n): l for k, n, _, l in items_a}
    dur_b = {(k, n): l for k, n, _, l in items_b}
    for key in sorted(set(dur_a) | set(dur_b)):
        if dur_a.get(key) != dur_b.get(key):
            print('DURATION: %s %s ms -> %s ms' % (name(*key),
                  round(dur_a.get(key, -1) * 1000),
                  round(dur_b.get(key, -1) * 1000)))
            failed += 1

    segs_a = segments(items_a, pcm_a)
    segs_b = segments(items_b, pcm_b)
    print('%-22s %7s %6s %6s %7s' % ('', 'level', 'shape', 'tone', 'length'))
    for key in sorted(set(segs_a) | set(segs_b)):
        a, b = segs_a.get(key), segs_b.get(key)
        if a is None or b is None:
            print('%-22s %s' % (name(*key), 'SILENT in ' +
                  ('both' if a is None and b is None else
                   'before' if a is None else 'after')))
            failed += 1
            continue
        r = compare(a, b)
        if key[0] == 'm':
            r['length'] = 0     # Music is stopped after a fixed time.
        bad = (abs(r['level']) > MAX_LEVEL_DB or r['shape'] < MIN_SHAPE or
               r['tone'] < MIN_TONE or abs(r['length']) > MAX_LENGTH_MS)
        print('%-22s %+6.1fdB %6.3f %6.3f %+5dms%s' % (name(*key),
              r['level'], r['shape'], r['tone'], r['length'],
              '  FAIL' if bad else ''))
        if bad:
            failed += 1
            if diffdir:
                os.makedirs(diffdir, exist_ok=True)
                base = os.path.join(diffdir, name(*key).replace(' ', '-'))
                write_wav(base + '-before.wav', a)
                write_wav(base + '-after.wav', b)

    if failed:
        print('%d audio differences.' % failed)
        return 1
    print('All %d sounds & tracks match.' % len(segs_a))
    return 0


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    sys.exit(main(*sys.argv[1:]))
