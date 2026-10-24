options [
	sdk_dir: none		"Path to Boron & SDL3 headers and libraries (UNIX only)"
	gpu_render: false
	make_util: true
]

libxml2: does [
	unix [
		include_from %/usr/include/libxml2
		libs %xml2
	]
	win32 [
		either msvc [
			cflags "/DLIBXML_STATIC"
			libs_from %../usr/lib [%libxml2_a]
		][
			libs %xml2
		]
	]
]

exe %xu4 [
	include_from [%src %src/lzw %src/support]
	unix [
		if sdk_dir [
			include_from join sdk_dir %/include
			lflags rejoin ["-L" sdk_dir %/lib]
		]
	]
	win32 [
		include_from %../usr/include
		include-define "_WIN32"		; Needed to archive glad.*
	]

	libs %SDL3
	sources_from %src [
		%screen_sdl3.cpp
		%sound_sdl3.cpp
	]

	;if use_boron [
		cflags "-DUSE_BORON -DCONF_MODULE"
		unix [
			libs %boron
			if sdk_dir [
					; Needed for static libboron.
					libs [%pthread]
			]
		]
		win32 [
			libs_from %../usr/lib either msvc %libboron %boron
			libs %ws2_32
		]
		sources_from %src [
			%config_boron.cpp
			%module.c
			%support/cdi.c
		]
	;]

	if gpu_render [cflags "-DGPU_RENDER"]
	cflags "-DUSE_GL"
	; OpenGL functions are obtained through SDL.
	macx [opengl]

	unix [
		cflags "-Wno-unused-parameter"
		libs %z
	]
	win32 [
		either msvc [
			libs_from %../usr/lib [%zlib]
			libs [%User32]
		][
			cflags "-Wno-unused-parameter"
			;lflags "-static-libgcc"
			lflags "-static-libstdc++"
			libs %z
		]
		include_from %src/win32
		sources/flags [%src/win32/xu4.rc] "-I src/win32"
	]
	cflags {-DVERSION=\"DR-1.0\"}

	sources_from %src [
		%a2disk.cpp
		%a2music.cpp
		%c64disk.cpp
		%c64music.cpp
		%smsmusic.cpp
		%smsrom.cpp
		%smsz80.cpp
		%annotation.cpp
		%aura.cpp
		%camp.cpp
		%cheat.cpp
		%city.cpp
		%codex.cpp
		%combat.cpp
		%controller.cpp
		%context.cpp
		%creature.cpp
		%death.cpp
		%debug.cpp
		%direction.cpp
		%discourse.cpp
		%docreader.cpp
		%docs.cpp
		%dungeon.cpp
		%dungeonview.cpp
		%error.cpp
		%event.cpp
		%filesystem.cpp
		%game.cpp
		%gamepad.cpp
		%gamebrowser.cpp
		%gui.cpp
		%image.cpp
		%imageloader.cpp
		%imagemgr.cpp
		%imageview.cpp
		%intro.cpp
		%item.cpp
		%location.cpp
		%map.cpp
		%maploader.cpp
		%menu.cpp
		%menuitem.cpp
		%names.cpp
		%object.cpp
		%party.cpp
		%person.cpp
		%portal.cpp
		%progress_bar.cpp
		%rle.cpp
		%savegame.cpp
		%scale.cpp
		%screen.cpp
		%settings.cpp
		%shrine.cpp
		%spell.cpp
		%stats.cpp
		%textview.cpp
		%tileanim.cpp
		%tile.cpp
		%tileset.cpp
		%tileview.cpp
		%u4file.cpp
		%view.cpp
		%xu4.cpp

		%lzw/hash.c
		%lzw/lzw.c
		%lzw/u6decode.cpp
		%lzw/u4decode.cpp

		%support/notify.c
		%support/sfx_gen.c
		%support/stb_vorbis.c
		%support/stringTable.c
		%support/txf_draw.c
		%support/unzip.c
		%support/well512.c
	]
]

if make_util [
	exe %coord   [console sources [%src/util/coord.c]]
	exe %tlkconv [console libxml2 sources [%src/util/tlkconv.c]]
	exe %dumpmap [console sources [%src/util/dumpmap.c]]
	exe %dumpsavegame [
		console
		include_from %src
		sources [
			%src/util/dumpsavegame.cpp
		]
	]
]
