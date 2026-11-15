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
; & character creation, which the intro plays as its title & story tunes.
; Where each tune plays was read from the game's music table (bank 14,
; $8000 & $8712): tunes 14 & 15 follow the balloon & ship, 12 the fights in
; dungeons, 13 the altar rooms & 3 the hall of the Codex.  The jingles were
; placed by their code: 20 is next to the experience table (level up), 22
; reads the party size (joined) and 23 is with the inn.  28 is the bell: it
; plays before the first step of the Abyss rite (bell, book, candle).  29 is
; the horn: it plays before the check for the daemons by Humility's shrine.
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
    59
    "nes/music:1"   ; Title (MUSIC_TITLE)
    "nes/music:2"   ; Story & gypsy (MUSIC_STORY)
    "nes/music:15"  ; Sailing (MUSIC_SAILING)
    "nes/music:14"  ; Balloon ride (MUSIC_BALLOON)
    "nes/music:12"  ; Fights in dungeons (MUSIC_DUNGEON_COMBAT)
    "nes/music:13"  ; Altar rooms (MUSIC_ALTAR)
    "nes/music:3"   ; Codex chamber; the NES plays it in the hall before it
    "nes/music:4"   ; Ending (MUSIC_ENDING)
    "nes/music:19"  ; Death (MUSIC_DEATH)
    "nes/music:20"  ; Level up (MUSIC_LEVEL_UP)
    "nes/music:22"  ; Joined party (MUSIC_JOINED)
    "nes/music:23"  ; Inn (MUSIC_INN)
    "nes/music:28"  ; Bell of Courage (MUSIC_BELL)
    "nes/music:29"  ; Silver Horn (MUSIC_HORN)
    ; Places with their own tune (MUSIC_PLACE 80 + map id).  The villages
    ; have the village tune and the Abyss its own dungeon tune.
    92
    "nes/music:7"   ; Paws (13)
    94
    "nes/music:7"   ; Vesper (15)
    "nes/music:7"   ; Cove (16)
    103
    "nes/music:11"  ; The Abyss (24)
]
