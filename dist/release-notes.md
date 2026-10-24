XU4 Legacy Edition is a fork of [XU4](https://xu4.sourceforge.net/) that
makes Ultima IV easier to play on modern systems, especially with the free
[GOG](https://www.gog.com/en/game/ultima_iv_quest_of_the_avatar) release.

## Getting started (Windows)

1. Install Ultima IV from GOG (free), or have the DOS game files ready.
2. Unzip `xu4-legacy-*-windows.zip` anywhere and run `xu4.exe`.

The game files are found automatically in `C:\GOG Games\Ultima 4 - Quest of
the Avatar`.  Otherwise put them (or `ultima4.zip`) next to `xu4.exe`, or in
`%LOCALAPPDATA%\xu4`.

## Getting started (Linux)

- **Archive:** unpack `xu4-legacy-*-linux.tar.gz` and run `./xu4` in that
  directory, or `./setup install` to install it for all users.
- **Debian & Ubuntu:** `sudo apt install ./xu4-legacy_*_amd64.deb`
  (replaces the distribution's xu4 package if installed).
- **Flatpak (any distribution):**
  `flatpak install --user xu4-legacy-*-x86_64.flatpak`, then start it from
  the menu or with `flatpak run io.github.alexdia25.xu4_legacy_edition`.
  The Freedesktop runtime is fetched from Flathub if needed.

Put the DOS game files (or `ultima4.zip`) in `~/.local/share/xu4`, or in
the directory xu4 is run from (the Flatpak only looks in
`~/.local/share/xu4`).

The source code is in `xu4-legacy-*.tar.gz`.

## What's new compared with XU4

- **Gamepad support** through SDL3.  The console style menus switch on by
  themselves when a controller is used.  A opens the action menu and
  picks, B cancels, Y shows stats, Select opens the xu4 menu and
  Start pauses.
- **Console (SMS) style menus**, also available for keyboard play under
  Configure > Enhanced Interface Options: an action menu instead of one
  key per command, conversation topics that are listed as they are
  discovered, and pickers in place of typed input.
- **Documents in the game.**  The ULTIMA4.TXT file of the GOG release is
  split into the History, Book of Mystic Wisdom, Hints and Reference
  Card, which can be read from the Docs button of the Escape (Select)
  menu.
- **Apple II graphics & music.**  With the four Ultima IV Apple II disk
  images (.dsk, DOS 3.3 order) next to the game files, the Apple-II module
  shows the Apple title, intro, tiles, font, line drawn dungeons, shrine
  and ending pictures, and the Apple-II-Music soundtrack plays the
  Mockingboard tunes.  Both are read from the disks when the game runs.
  See "Apple II Disk Images" in the README.
- **Commodore 64 graphics & music.**  With the Program disk image (.d64)
  of the C64 release next to the game files, the C64 module shows the C64
  title, intro pictures, tiles, font and virtue cards, and the C64-Music
  soundtrack plays the SID tunes through the game's own music player.  The
  C64-Remastered module uses the tiles & intro art of the Ultima IV
  Remastered fan release (u4remastered.d81).  See "C64 Disk Images" in the
  README.
- **Sega Master System graphics & music.**  With the ROM of the Master
  System release (.sms) next to the game files, the SMS module shows its
  title screen, map tiles, font, story pictures and the gypsy's cards, and
  the SMS-Music soundtrack plays its tunes through the game's own sound
  driver.  See "SMS ROM" in the README.
- The module browser has separate Graphics and Music sections, so any
  soundtrack can go with any graphics, and lists only modules whose game
  data is present.
- Point (pixel sharp) scaling is the default video filter.

Gamepad play has not yet been tested widely; please report problems in
the issue tracker.

No game data is included.  XU4 is licensed under the GNU GPL version 3.
