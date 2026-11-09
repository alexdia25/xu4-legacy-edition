module [
    about: {{
        The music of the NES release of Ultima IV.
        Requires the ROM of Ultima: Quest of the Avatar for the NES (.nes)
        in the xu4 data folder, next to the DOS game files.
    }}
    author: "XU4 Legacy Edition"
    version: "0.1"
    rules: "Ultima-IV/1.4"
    requires: "nes"
]

; The tunes are played by the game's own sound driver on an emulated 6502 &
; sound unit.  The NES keeps the castle tune while talking to Lord British
; and Hawkwind, who are in his castle.  Tune 1 is the title and 2 the story
; & character creation.
music: [
    "nes/music:5"   ; Outside
    "nes/music:6"   ; Towns
    "nes/music:9"   ; Shrines
    "nes/music:8"   ; Shopping (Hawkwind)
    "nes/music:8"   ; Rule Britannia (Lord British)
    "nes/music:2"   ; Fanfare
    "nes/music:10"  ; Dungeons
    "nes/music:16"  ; Combat
    "nes/music:8"   ; Castles
]
