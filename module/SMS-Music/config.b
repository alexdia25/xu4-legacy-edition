module [
    about: {{
        The music of the Sega Master System release of Ultima IV.
        Requires the ROM of Ultima IV: Quest of the Avatar for the Master
        System (.sms) in the xu4 data folder, next to the DOS game files.
    }}
    author: "XU4 Legacy Edition"
    version: "0.1"
    rules: "Ultima-IV/1.4"
    requires: "sms"
]

; The tunes are played by the game's own sound driver on an emulated Z80 &
; PSG.  Lord British's tune also plays at Journey Onward, and the SMS
; plays the town tune at the title.
music: [
    "sms/music:5"   ; Outside
    "sms/music:1"   ; Towns
    "sms/music:3"   ; Shrines
    "sms/music:4"   ; Shopping
    "sms/music:7"   ; Rule Britannia
    "sms/music:7"   ; Fanfare
    "sms/music:8"   ; Dungeons
    "sms/music:2"   ; Combat
    "sms/music:6"   ; Castles
]
