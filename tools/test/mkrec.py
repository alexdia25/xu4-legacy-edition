#!/usr/bin/env python3
"""
Convert a test session script into an xu4 input recording (for --replay).

Script lines (# starts a comment):
    seed N          Random seed (must come first; default 1).
    wait N          Wait N frames before the next key.
    gap N           Frames between keys from now on (default 4).
    key NAME...     Press keys: a single character, or one of
                    up down left right enter esc space tab backspace,
                    f1-f12, alt-X, ctrl-X, pad-start pad-b pad-select
                    pad-l pad-r.
    type TEXT       Type the rest of the line as characters.
    snap            Save a screenshot (to XU4_TEST_DIR) of the next frame.
    repeat N KEY    Press KEY N times.
    include FILE    Read commands from FILE (relative to this script).

Lines starting with "#!" are directives for run.sh (e.g. "#! module
U4-Upgrade").

Usage: mkrec.py session.txt out.rec
"""

import os
import struct
import sys

RECORD_CDI = 0xC04F7ADA     # Little-endian (see src/support/irecord.c).
RECORD_KEY = 1
RECORD_END = 0xff

U4_ALT = 0x080
U4_FKEY = 0x13a
NAMES = {
    'up': ord('['), 'down': ord('/'), 'left': ord(';'), 'right': ord("'"),
    'enter': 13, 'esc': 27, 'space': 32, 'tab': 9, 'backspace': 8,
    'pad-start': 0x1F0, 'pad-b': 0x1F1, 'pad-select': 0x1F2,
    'pad-l': 0x1F3, 'pad-r': 0x1F4,
    'snap': 0x1FE,
}


def keycode(name):
    if len(name) == 1:
        return ord(name)
    low = name.lower()
    if low in NAMES:
        return NAMES[low]
    if low.startswith('alt-') and len(name) == 5:
        return U4_ALT | ord(name[4])
    if low.startswith('ctrl-') and len(name) == 6:
        return ord(low[5]) & 0x1f
    if low[0] == 'f' and low[1:].isdigit():
        return U4_FKEY + int(low[1:]) - 1
    raise ValueError('Unknown key ' + name)


def main(src, dst):
    seed = 1
    gap = 4
    delay = 0
    events = []

    def press(code):
        nonlocal delay
        mod = (code & U4_ALT) >> 4
        events.append(struct.pack('<BBHH', RECORD_KEY, mod, code & ~U4_ALT,
                                  delay))
        delay = gap

    def lines(path):
        for num, line in enumerate(open(path), 1):
            stripped = line.strip()
            if stripped.startswith('include '):
                inc = os.path.join(os.path.dirname(path), stripped[8:].strip())
                yield from lines(inc)
            else:
                yield path, num, line

    for src, num, line in lines(src):
        line = line.rstrip('\n')
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue
        cmd, _, arg = stripped.partition(' ')
        try:
            if cmd == 'seed':
                seed = int(arg)
            elif cmd == 'wait':
                delay += int(arg)
            elif cmd == 'gap':
                gap = int(arg)
            elif cmd == 'key':
                for k in arg.split():
                    press(keycode(k))
            elif cmd == 'type':
                text = line[line.index('type ') + 5:]
                for ch in text:
                    press(ord(ch))
            elif cmd == 'repeat':
                n, k = arg.split()
                for _ in range(int(n)):
                    press(keycode(k))
            elif cmd == 'snap':
                press(NAMES['snap'])
            else:
                raise ValueError('Unknown command ' + cmd)
        except ValueError as e:
            sys.exit('%s:%d: %s' % (src, num, e))

    # Let the last wait run before the recording ends (a zero key is
    # ignored by the replay).
    press(0)
    with open(dst, 'wb') as fp:
        fp.write(struct.pack('<II', RECORD_CDI, seed))
        fp.write(b''.join(events))
        fp.write(bytes([RECORD_END]))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
