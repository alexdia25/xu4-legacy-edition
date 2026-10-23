XU4 Legacy Edition
==================

XU4 Legacy Edition is a fork of [XU4], the remake of Ultima IV that runs
the original DOS game data on modern systems.  All credit for XU4 itself
goes to the original XU4 team (see [AUTHORS](AUTHORS)).  Official XU4
releases are on the [XU4 download page], and its development continues
on [Codeberg].

This fork aims to make Ultima IV smoother and easier to get into,
especially for someone who has just downloaded the free release from
[GOG].  Planned and in-progress work:

 - **Controller support.**  A platform independent gamepad layer, with a
   new SDL3 backend (`./configure --sdl3`).  The Select (Back, −)
   button opens and closes the xu4 menu, and Start pauses.
 - **Console style menus.**  An optional interface modelled on the Sega
   Master System version: an action menu instead of one key per command,
   conversation topics that are discovered and listed rather than typed,
   and pickers for spells, items, numbers and words, so the whole game
   can be played with a D-pad and two buttons.  It switches on by itself
   when a gamepad is used, or can be turned on for keyboard play with
   Configure > Enhanced Interface Options > Console (SMS) Menus.
 - **Mouse-only and touch play.**  The console menus can also be played
   with just taps (left clicks), as a step towards touch screens: tap
   the map to move toward the tap (hold to keep walking, drag to steer),
   tap your party or the message area for the action menu, tap the party
   list for the Ztats, and tap menu items, letters and numbers to pick
   them (tap outside a list to cancel).  Direction prompts take a tap
   toward the target, the gypsy's questions a tap on a card, and the
   action menu's Game Menu entry opens the xu4 menu.  No right button or
   hovering is needed.  The first tap switches the console menus on.
 - **Documents in the game.**  Bringing the out-of-game material (the
   History of Britannia, the Book of Mystic Wisdom, the cloth map) into
   the flow of play, so the game can be played without leaving XU4.
   The ULTIMA4.TXT file of the GOG release is split into separate
   documents, which can be read from the Docs button of the Escape
   menu (D key, or Y on a gamepad).  Planned: the Book of Mystic Wisdom
   opening automatically at the right page during play (spells,
   reagents, virtues), and the History of Britannia offered when the
   game prompts for it.
 - **Pixel-sharp graphics by default.**  The video filter defaults to
   point scaling instead of HQX.
 - **Apple II graphics & music.**  The Apple-II module and Apple-II-Music
   soundtrack read the art and Mockingboard tunes of the Apple II release
   straight from its disk images (see
   [Apple II Disk Images](#apple-ii-disk-images)).
 - **Commodore 64 graphics & music.**  The C64 module and C64-Music
   soundtrack read the art and SID tunes of the C64 release from its disk
   images, and the C64-Remastered module uses the art of the Ultima IV
   Remastered fan release (see [C64 Disk Images](#c64-disk-images)).
 - **More platforms.**

Like XU4, this fork needs the Ultima IV for DOS data files, which are
freeware from [GOG].  No game data is stored in this repository.

### Screenshots

The DOS graphics with the console style menus: the action menu, and the
topics of a conversation.

![DOS graphics with the console action menu](doc/screenshots/dos-action-menu.png)
![DOS graphics with conversation topics](doc/screenshots/dos-talk-topics.png)

The Apple-II module, using the tiles of the Apple II release: the
overworld near Britain, and the town of Yew.

![Apple II tiles on the overworld](doc/screenshots/apple2-overworld.png)
![Apple II tiles in Yew, with the action menu](doc/screenshots/apple2-town.png)

The C64 module (top) and the C64-Remastered module (bottom): the title,
the moongate of the intro, the gypsy's cards and the first view of
Britannia.

![C64 and C64-Remastered modules](doc/screenshots/c64-modules.png)

The original XU4 readme follows.

[XU4]: https://xu4.sourceforge.net/
[XU4 download page]: https://xu4.sourceforge.net/download.html
[Codeberg]: https://codeberg.org/xU4/xu4
[GOG]: https://www.gog.com/en/game/ultima_iv_quest_of_the_avatar


XU4
===

> Prepare yourself for a grand adventure: Ultima IV, sixteen times
> larger than Ultima III, is a milestone in computer gaming.  Lord
> British has produced a game to challenge, not only your physical
> and mental skills, but the true fabric of your character.  The evil
> triad of Mondain, Minax, and the hellspawn Exodus, have been
> vanquished and peace reigns throughout the land of Britannia.  Evil
> yet abounds, but in isolated pockets and in the hearts of men.  A
> new age awaits the coming of one who can conquer evil on all
> frontiers through the mastery of both magic and the use of force.
> Daemons, dragons and long-dead wizards still plague the countryside
> and must be destroyed.  The seeker on the path of the avatar will
> faces hostile groups composed of mixed enemy types and will survive
> such encounters only by strategic use of weapons and terrain.
> Earthly victories over seemingly impossible odds lead to the final
> conflict, where the ultimate challenge -- the self -- awaits...
>
>   -- Back cover of Ultima IV box

XU4 is a remake of the computer game Ultima IV.  The goal is to make
it easy and convenient to play this classic on modern operating
systems.  XU4 is primarily inspired by the much more ambitious project
Exult.  Linux is the primary development platform but it gets built
for Windows regularly.  It should be easy to port to any system with
[Allegro] 5.2 or [GLFW] 3 support.

XU4 isn't a new game based on the Ultima IV story -- it is a faithful
recreation of the old game, right up to the crappy graphics.  If you
are looking for a game with modern gameplay and graphics, this is not
it -- yet.  New features that improve the gameplay and keep with the
spirit of the original game will be added.

XU4 also tries to maintain strict compatibility with the original for
its savegame files.  You can use a game saved in XU4 with the original
and vice versa, at least in theory.


Status
------

The game is fully playable and can use the original DOS EGA and
upgraded VGA graphics.

Some optional features have been added such as reagent mixing similar to
Ultima 5, speech for non-player characters, and unique spell sounds.

Some thoughts for possible improvements:
 - Ultima 5 style aiming in combat (i.e. allow angle shots)
 - More sound effects
 - Support for higher-resolution tile sets
 - Allow the map view to display more of the world
 - Menu-based interface, like Sega version
 - Use GPU for all rendering.
 - Formal modding system to extend the world or create entirely new adventures.


Compiling
---------

To build the binary on Linux use these commands:

    ./configure
    make

If the required libraries & headers are present, make will create the
executable `src/xu4`.

For more detailed build instructions see [doc/build.md](doc/build.md).


Running
-------

The actual data files from Ultima 4 are loaded at runtime, which means
that a copy of Ultima 4 for DOS must be present.  Fortunately, Ultima IV is
available as closed-source freeware from [www.gog.com].

Some graphics and sounds are stored in module files which have a `.mod`
extension.  Soundtrack modules that only contain music streams are also
available.  The Game Module Browser, opened with the Escape key, lists them
in two sections: Graphics, where one game module is chosen, and Music, where
one soundtrack module (or the game's own music) is chosen.  Space or a click
picks a module and Enter or Play starts with the checked pair.  Modules can be
found on the [XU4 Download] page.

If you have the optional u4upgrad.zip XU4 can read the data files straight
out of the zipfile.  To use it select the U4-Upgrade module.

The Apple-II module uses the graphics of the original Apple II release, and
the Apple-II-Music soundtrack plays its Mockingboard music (see
[Apple II Disk Images](#apple-ii-disk-images) below).  The C64, C64-Music &
C64-Remastered modules do the same for the Commodore 64 release (see
[C64 Disk Images](#c64-disk-images)).

XU4 searches for data, modules and zipfiles in the following places:
 - The current directory when xu4 is run
 - On Linux: `$HOME/.local/share/xu4`, `/usr/share/xu4` & `/usr/local/share/xu4`
 - On Windows: `%LOCALAPPDATA%\xu4` & `C:\GOG Games\Ultima 4 - Quest of the Avatar`
   (where the GOG release installs by default)

The Flatpak can only see `~/.local/share/xu4` (for the game data, extra
modules and the Apple II disk images) and `~/.config/xu4` (settings and
saved games), so put the files there.

The DOS game zipfiles (ultima4.zip & u4upgrad.zip) don't need to be unpacked,
but if they are the contents may be in a subdirectory named `ultima4` or `u4`.

### Apple II Disk Images

The Apple-II module reads its graphics straight from the disk images of
Ultima IV for the Apple II, and the Apple-II-Music soundtrack reads the
Mockingboard tunes from them.  These are only listed in the Game Module
Browser when all four disks are found.  The DOS game files are still needed for
everything else.

The release came on two floppy disks, used on both sides, so there are four
disk images:

| Disk side | Contents                          |
|-----------|-----------------------------------|
| Program    | Title screen, intro pictures, tiles & font |
| Britannia  | World map, combat maps & shrines           |
| Towne      | Towns, castles & conversations             |
| Underworld | Dungeons, the Abyss & the ending           |

Each image must be a 140 KB (143,360 byte) file in DOS 3.3 sector order,
which usually has a `.dsk` or `.do` extension.  Images in other formats
(`.po`, `.nib`, `.woz`, `.2mg`) are not read; tools such as CiderPress can
convert them.  Cracked copies such as the 4am crack work.

The file names don't matter, since xu4 tells the disks apart by the files on
them.  Put the four images in any of the places listed above, or in an
`apple2` folder inside one of them.  For example:

 - Windows: `%LOCALAPPDATA%\xu4\apple2\`
 - Linux: `~/.local/share/xu4/apple2/`

Other files that come with the images (text notes, scans, `.woz` copies)
aren't needed.

### C64 Disk Images

The C64 module reads the title screen, intro pictures, tiles, font and
virtue cards straight from the disk images of Ultima IV for the Commodore
64, and the C64-Music soundtrack plays its SID music by running the game's
own music player.  Both need only the Program disk; the dungeons, shrines
and ending still use the DOS pictures.

The C64 release also came on two floppy disks used on both sides (Program,
Britannia, Towne & Underworld), so a full set is four `.d64` images, though
some sets name them "Disk 1" and "Disk 2".  The images are told apart by
their contents, so the file names don't matter, and cracked copies work.
The images must be plain 35 track `.d64` files.

The C64-Remastered module uses the graphics of
[Ultima IV Remastered](https://github.com/MagerValp/u4remastered) 2.2.1 by
Per Olofsson & Vanja Utne: the more colorful tiles and the new intro
pictures and cards.  It reads them from `u4remastered.d81`, or from
`u4remastered-a.d64`.

Put the images in any of the places listed above, or in a `c64` folder
inside one of them, such as `%LOCALAPPDATA%\xu4\c64\` on Windows.  The
modules are only listed in the Game Module Browser when their disk images
are found.

### Settings & Options

At the title screen, a configuration menu can be accessed by pressing
'c'.  Here, the screen scale, filter, volume and other settings can be
modified.  These settings are stored in `$HOME/.config/xu4/xu4rc` on Linux
and `%APPDATA%\xu4\xu4.cfg` on Windows.

The saved game files are stored in the settings directory.

XU4 also accepts the following command line options:

        --filter <string>   Specify display filtering mode.
                            (point, HQX, xBR-lv2, xBRZ, point-43, xBRZ-43)
    -f, --fullscreen        Run in fullscreen mode.
    -h, --help              Print this message and quit.
    -i, --skip-intro        Skip the intro. and load the last saved game.
    -m, --module <file>     Specify game module (default is Ultima-IV).
    -p, --profile <string>  Use another set of settings and save files.
    -q, --quiet             Disable audio.
    -s, --scale <int>       Specify display scaling factor (1-5).
    -v, --verbose           Enable verbose console output.

### Profiles

Profiles are stored in the `profiles` sub-directory.
Use quotation marks around profile names that include spaces.
The active profile name is shown on the introduction map view off the main menu.


Ultima 4 Documentation
----------------------

Included with Ultima 4 for DOS, as downloaded from one of the above
sites, are electronic copies of the printed documentation from the
original Ultima IV box.  HISTORY.TXT contains the "The History of
Britannia", a general introduction to the world of Ultima IV.
WISDOM.TXT contains "The Book of Mystic Wisdom", which explains the
system of magic and provides descriptions of the spells and reagents.

PDF versions of these books are included in the official download in
the EXTRA folder of the zip file.
As an added bonus this folder also includes a pdf of the offical 
Ultima IV cluebook.

An image of the cloth map from the original Ultima IV box is also
included in the EXTRA folder.


Debug Mode (cheats)
-------------------

XU4 has a very useful debug mode (you can also think of it as a cheat mode).
To enable it:
- press 'c' in the main menu
- make sure that
  1) Game Enhancements = On
  2) Enhanced Gameplay Options -> Debug Mode (Cheats) = On

Cheat list:
* ctrl-c (cheat menu; press one of the following keys to use a cheat)

        1-8   gate (teleports you to a moongate location)
        F1-F8 virtue +10
        a     advance moons (advances the left moon [Trammel] to its next phase)
        c     collision (lets you walk across water, through mountains, etc.)
        e     equipment (gives the party Armour and Weapons)
        f     full stats (gives all party members 50 str, dex & intel and level 8)
        g     goto (enter a location and you will be teleported there)
        h     help (displays list of available cheats)
        i     items (gives the party Items and Equipment)
        j     joined by companions (if eligible)
        k     show karma (shows your virtues)
        l     location (displays current map and coordinates)
        m     mixtures (gives the party 99 mixtures of all spells)
        o     opacity (lets you see through opaque tiles)
        p     peer (switches between normal and gem view)
        r     reagents (gives the party 99 of all reagents)
        s     summon (enter a monster name, and it will appear somewhere nearby)
        t     transports (press b/h/s + arrow key to create a balloon/horse/ship)
        v     full virtues (makes you a full avatar)
        w     change wind (changes or locks the wind direction)
        x     exit map (teleports the party to where it entered the current map)
        y     y-up (like the Y-up spell, but free)
        z     z-down (like the Z-down spell, but free)

* ctrl-d (destroy monster/object; doesn't work on energy fields)
* ctrl-e (end combat)
* ctrl-h (teleport to Lord British's throne room)
* ctrl-v (switch between 3-d and 2-d view; dungeons only)
* F1-F8 (teleport to a dungeon entrance; surface only)
* F9-F11 (teleport to the altar room of truth/love/courage; surface only)
* F12 (display torch duration)

Note:
Except for the ctrl-e command, none of the cheats work during combat.


Misc.
-----

See http://xu4.sourceforge.net/links.html for some other interesting
Ultima IV related links.

Please send an email to wickedsmoke@users.sourceforge.net if you are
interested in helping.


[Allegro]: https://liballeg.org/
[GLFW]: https://www.glfw.org
[www.gog.com]: https://www.gog.com/en/game/ultima_iv_quest_of_the_avatar
[XU4 Download]: https://xu4.sourceforge.net/download.html#release
