XU4 Legacy Edition is a fork of [XU4](https://xu4.sourceforge.net/) that
makes Ultima IV easier to play on modern systems, especially with the free
[GOG](https://www.gog.com/en/game/ultima_iv_quest_of_the_avatar) release.

## Getting started (Windows)

1. Install Ultima IV from GOG (free), or have the DOS game files ready.
2. Unzip this archive anywhere and run `xu4.exe`.

The game files are found automatically in `C:\GOG Games\Ultima 4 - Quest of
the Avatar`.  Otherwise put them (or `ultima4.zip`) next to `xu4.exe`, or in
`%LOCALAPPDATA%\xu4`.

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
- Point (pixel sharp) scaling is the default video filter.

Gamepad play has not yet been tested widely; please report problems in
the issue tracker.

No game data is included.  XU4 is licensed under the GNU GPL version 3.
