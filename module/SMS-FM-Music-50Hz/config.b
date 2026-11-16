module [
    about: {{
        The FM Sound Unit music of the Sega Master System release of
        Ultima IV, the game's second soundtrack, for the YM2413 sound chip,
        as it plays on a 50 Hz (PAL) console.
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
; The tunes are played by the game's own sound driver on an emulated Z80,
; told that the FM Sound Unit is fitted, & on a model of the YM2413.  Lord
; British's tune also plays at Journey Onward, and the SMS plays the town
; tune at the title.
music: [
    "sms/fm50:5"      ; Outside
    "sms/fm50:1"      ; Towns
    "sms/fm50:3"      ; Shrines
    "sms/fm50:4"      ; Shopping
    "sms/fm50:7"      ; Rule Britannia
    "sms/fm50:7"      ; Fanfare
    "sms/fm50:8"      ; Dungeons
    "sms/fm50:2"      ; Combat
    "sms/fm50:6"      ; Castles
]
