module [
    about: {{
        The FM Sound Unit music of the Sega Master System release of
        Ultima IV, the game's second soundtrack, for the YM2413 sound chip.
        Requires the ROM of Ultima IV: Quest of the Avatar for the Master
        System (.sms) in the xu4 data folder, next to the DOS game files.
    }}
    author: "XU4 Legacy Edition"
    version: "0.1"
    rules: "Ultima-IV/1.4"
    requires: "sms"
]

; The tunes are played by the game's own sound driver on an emulated Z80,
; told that the FM Sound Unit is fitted, & on a model of the YM2413.  Lord
; British's tune also plays at Journey Onward, and the SMS plays the town
; tune at the title.
music: [
    "sms/fm:5"      ; Outside
    "sms/fm:1"      ; Towns
    "sms/fm:3"      ; Shrines
    "sms/fm:4"      ; Shopping
    "sms/fm:7"      ; Rule Britannia
    "sms/fm:7"      ; Fanfare
    "sms/fm:8"      ; Dungeons
    "sms/fm:2"      ; Combat
    "sms/fm:6"      ; Castles
]
