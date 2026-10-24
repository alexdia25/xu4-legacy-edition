XU4 Build Instructions
======================

This document describes the various ways to build binaries & packages.

The following build systems are provided:

 - [GNU Make] for developers.
 - [Copr] for developers.
 - The `dist/build-linux.sh` & `dist/build-windows.sh` scripts for release
   packages.

SDL3 is the only platform API.  It provides the window, OpenGL context,
input, gamepads, audio output & PNG loading.


Developer Builds
----------------

### Dependencies

The following libraries are required for all targets:

 - SDL 3.4 (or later)
 - Boron 2.0.8
 - zlib

Ogg Vorbis decoding (stb_vorbis) and the rFX sound effect synthesizer
(sfx_gen, from Faun) are included in src/support.

SDL 3.4 is newer than what most Linux distributions package, so it may need
to be built from [source](https://github.com/libsdl-org/SDL/releases)
(see the SDL [build dependencies] for Linux):

    cmake -S SDL3-3.4.18 -B sdl-build -DCMAKE_BUILD_TYPE=Release
    cmake --build sdl-build
    sudo cmake --install sdl-build

The Boron interpreter program is needed to build game modules.
The static binary can be downloaded from the
[Boron homepage](https://urlan.sourceforge.io/boron/) or from the
[Module Tools](https://xu4.sourceforge.net/download.html#devel) archive and
should be installed somewhere in your PATH.

There are three ways the Boron library can be obtained.  It can be built from
[source](https://sourceforge.net/p/urlan/boron/code/ci/master/tree/)
or a pre-built x86_64 SDK can be downloaded from the
[xu4 download](https://xu4.sourceforge.net/download.html#devel) page.
On UNIX systems the following commands will checkout the source using Git,
properly configure it for xu4, build `libboron.a`, and install it:

    git clone -b v2.0.8 https://github.com/0branch/boron
    cd boron && ./configure --no-execute --no-socket --static --thread
    make && sudo make DESTDIR=/usr install-dev

If the Boron program has not been installed yet then adding the `install`
target to the previous make command will do that.

### configure

The configure script can be used to customize both Make & Copr builds.
The default options match what is used for release builds.
Configure does not need to be run if this is what you want.
To see the configure options run:

    ./configure -h


### GNU Make

The following commands should get you running:

    ./configure
    make download
    make
    src/xu4

This downloads the original game archives, builds the binary, packs the game
modules, and then runs the program.

### Copr

The following commands should get you running:

    ./configure
    make download
    make mod
    copr
    ./xu4


Release Builds
--------------

The release archives are built by the Release workflow on GitHub
(.github/workflows/release.yml), which runs these scripts on Ubuntu:

    make mod
    dist/build-linux.sh <version>
    dist/build-windows.sh <version>

The Linux script builds SDL3 from source and links it statically; the
Windows script cross-compiles with MinGW and ships SDL3.dll.  Dependencies
are downloaded to build-linux/ & build-win/.


[GNU Make]: https://www.gnu.org/software/make/manual/html_node/index.html#toc-Overview-of-make
[Copr]: http://urlan.sourceforge.io/copr.html
[build dependencies]: https://wiki.libsdl.org/SDL3/README-linux#build-dependencies


Flatpak
-------

The Flatpak manifest is `dist/flatpak/io.github.alexdia25.xu4_legacy_edition.yml`.
It uses the Freedesktop 25.08 runtime and builds SDL 3.4 & Boron from
source (the runtime's SDL3 is older than 3.4).  With `flatpak` &
`flatpak-builder` installed, this builds a single-file bundle in
`build-flatpak/` (the runtime & SDK are installed from Flathub first):

    dist/build-flatpak.sh [version]

To build & install it for the current user instead:

    flatpak-builder --user --install --force-clean build-flatpak/app \
        dist/flatpak/io.github.alexdia25.xu4_legacy_edition.yml

The sandbox may only use `~/.local/share/xu4` (game data, modules & Apple II
disk images) and `~/.config/xu4` (settings & saved games).
