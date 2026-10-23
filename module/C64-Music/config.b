module [
    about: {{
        The SID music of the original Commodore 64 release of Ultima IV.
        Requires the Program disk image (.d64) of Ultima IV for the C64 in
        the xu4 data folder, next to the DOS game files.
    }}
    author: "XU4 Legacy Edition"
    version: "0.1"
    rules: "Ultima-IV/1.4"
    requires: "c64"
]

; The tunes are read from the disks and played by the game's own music
; player on an emulated 6502 & SID.  Each disk area has a music file with
; its own first tune; the combat & shrine tunes are in all of them.
music: [
    "c64/program/mus1:1"    ; Outside
    "c64/program/mus0:1"    ; Towns
    "c64/program/mus1:3"    ; Shrines
    "c64/program/mus0:4"    ; Shopping
    "c64/program/mus4:1"    ; Rule Britannia
    "c64/program/mus4:1"    ; Fanfare
    "c64/program/mus2:1"    ; Dungeons
    "c64/program/mus1:2"    ; Combat
    "c64/program/mus3:1"    ; Castles
]
