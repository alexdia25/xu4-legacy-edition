module [
    about: {{
        The music of the Sega Master System release of Ultima IV, as it
        plays on a 50 Hz (PAL) console.
        Requires the ROM of Ultima IV: Quest of the Avatar for the Master
        System (.sms) in the xu4 data folder, next to the DOS game files.
    }}
    author: "XU4 Legacy Edition"
    version: "0.1"
    rules: "Ultima-IV/1.4"
    requires: "sms"
]

; Played as on a 50 Hz (PAL) console: the driver uses its PAL tempo table
; and the chips run on the slightly slower PAL clock.
; The tunes are played by the game's own sound driver on an emulated Z80 &
; PSG.  Lord British's tune also plays at Journey Onward, and the SMS
; plays the town tune at the title.
music: [
    "sms/music50:5"   ; Outside
    "sms/music50:1"   ; Towns
    "sms/music50:3"   ; Shrines
    "sms/music50:4"   ; Shopping
    "sms/music50:7"   ; Rule Britannia
    "sms/music50:7"   ; Fanfare
    "sms/music50:8"   ; Dungeons
    "sms/music50:2"   ; Combat
    "sms/music50:6"   ; Castles
]
