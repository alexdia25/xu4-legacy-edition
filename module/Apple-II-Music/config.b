module [
    about: {{
        The Mockingboard music of the original Apple II release of
        Ultima IV.  Requires the four Ultima IV Apple II disk images (.dsk)
        in the xu4 data folder, next to the DOS game files.
    }}
    author: "XU4 Legacy Edition"
    version: "0.1"
    rules: "Ultima-IV/1.4"
    requires: "apple2"
]

; The tunes are read from the disks and played as the Mockingboard would.
music: [
    "britannia/MUSO:1"      ; Outside
    "towne/MUST:1"          ; Towns
    "britannia/MUSO:3"      ; Shrines
    "towne/MUST:4"          ; Shopping
    "britannia/MUSB:1"      ; Rule Britannia
    "britannia/MUSB:1"      ; Fanfare
    "underworld/MUSD:1"     ; Dungeons
    "britannia/MUSO:2"      ; Combat
    "towne/MUSC:1"          ; Castles
]
