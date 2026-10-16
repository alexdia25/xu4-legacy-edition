graphics: [
    texture: atlas 64,4096 [
        tiles    0, 0
        hud     17, 0
        charset 17,33
    ]
    image (name: tiles filename: "a2/program/SHP0+SHP1" filetype: a2shapes tiles: 256 fixup: blackTransparencyHack) [
        at 0,0 size 14,16
        ; subimages
        tile_sea
        tile_water
        tile_shallows
        tile_swamp
        tile_grass
        tile_brush
        tile_forest
        tile_hills
        tile_mountains
        tile_dungeon
        tile_city
        tile_castle
        tile_town
        tile_lcb_west
        tile_lcb_entrance
        tile_lcb_east
        tile_ship   4
        tile_horse  2
        tile_dungeon_floor
        tile_bridge
        tile_balloon
        tile_bridge_n
        tile_bridge_s
        tile_up_ladder
        tile_down_ladder
        tile_ruins
        tile_shrine
        tile_avatar
        tile_mage       2
        tile_bard       2
        tile_fighter    2
        tile_druid      2
        tile_tinker     2
        tile_paladin    2
        tile_ranger     2
        tile_shepherd   2
        tile_column
        tile_waterside_sw
        tile_waterside_se
        tile_waterside_nw
        tile_waterside_ne
        tile_shipmast
        tile_shipwheel
        tile_rocks
        tile_corpse
        tile_stone_wall
        tile_locked_door
        tile_door
        tile_chest
        tile_ankh
        tile_brick_floor
        tile_wood_floor
        tile_moongate_opening   3
        tile_moongate
        tile_poison_field
        tile_energy_field
        tile_fire_field
        tile_sleep_field
        tile_solid
        tile_secret_door
        tile_altar
        tile_campfire
        tile_lava
        tile_miss_flash
        tile_magic_flash
        tile_hit_flash
        tile_guard      2
        tile_villager   2
        tile_bard_singing   2
        tile_jester     2
        tile_beggar     2
        tile_child      2
        tile_bull       2
        tile_lord_british   2
        tile_A
        tile_B
        tile_C
        tile_D
        tile_E
        tile_F
        tile_G
        tile_H
        tile_I
        tile_J
        tile_K
        tile_L
        tile_M
        tile_N
        tile_O
        tile_P
        tile_Q
        tile_R
        tile_S
        tile_T
        tile_U
        tile_V
        tile_W
        tile_X
        tile_Y
        tile_Z
        tile_space
        tile_space_r
        tile_space_l
        tile_window
        tile_black
        tile_brick_wall
        tile_pirate_ship    4
        tile_nixie      2
        tile_giant_squid    2
        tile_sea_serpent    2
        tile_sea_horse  2
        tile_whirlpool  2
        tile_twister    2
        tile_rat        4
        tile_bat        4
        tile_spider     4
        tile_ghost      4
        tile_slime      4
        tile_troll      4
        tile_gremlin    4
        tile_mimic      4
        tile_reaper     4
        tile_insect_swarm   4
        tile_gazer      4
        tile_phantom    4
        tile_orc        4
        tile_skeleton   4
        tile_rogue      4
        tile_python     4
        tile_ettin      4
        tile_headless   4
        tile_cyclops    4
        tile_wisp       4
        tile_evil_mage  4
        tile_liche      4
        tile_lava_lizard    4
        tile_zorn       4
        tile_daemon     4
        tile_hydra      4
        tile_dragon     4
        tile_balron     4
        at 0,1120 tile_fire_phantom 2
    ]
    image (name: charset filename: "a2/program/HTXT" filetype: a2font tiles: 256)
    ; Intro screens.  Apple screens are placed at x 20 of a 320x200 image.
    image (name: title filename: "a2/program/BGND" filetype: a2hgr fixup: apple2intro) [
        options_top  0, 32, 320,128
        options_btm  0,112, 320, 80
        t_signature   96, 0, 130,16
        t_and        152,17,  25, 4
        t_bar         90,31, 145, 1
        t_origin      87,21, 149, 9
        t_present    135,33,  56, 5
        t_title       60,34, 202,46
        t_subtitle    40,80, 241,13
        t_map          0,96, 320,96
    ]
    image (name: beasties filename: "a2/program/ANIM" filetype: a2hgr) [
        beast0frame00  20,  0, 49,31
        beast0frame01  20, 32, 49,31
        beast0frame02  20, 64, 49,31
        beast0frame03  20, 96, 49,31
        beast0frame04  20,128, 49,31
        beast0frame05  20,160, 49,31
        beast0frame06  69,  0, 49,31
        beast0frame07  69, 32, 49,31
        beast0frame08  69, 64, 49,31
        beast0frame09  69, 96, 49,31
        beast0frame10  69,128, 49,31
        beast0frame11  69,160, 49,31
        beast0frame12 118,  0, 49,31
        beast0frame13 118, 32, 49,31
        beast0frame14 118, 64, 49,31
        beast0frame15 118, 96, 49,31
        beast0frame16 118,128, 49,31
        beast0frame17 118,160, 49,31
        beast1frame00 258,  0, 42,31
        beast1frame01 258, 32, 42,31
        beast1frame02 258, 64, 42,31
        beast1frame03 258, 96, 42,31
        beast1frame04 258,128, 42,31
        beast1frame05 258,160, 42,31
        beast1frame06 216,  0, 42,31
        beast1frame07 216, 32, 42,31
        beast1frame08 216, 64, 42,31
        beast1frame09 216, 96, 42,31
        beast1frame10 216,128, 42,31
        beast1frame11 216,160, 42,31
        beast1frame12 174,  0, 42,31
        beast1frame13 174, 32, 42,31
        beast1frame14 174, 64, 42,31
        beast1frame15 174, 96, 42,31
        beast1frame16 174,128, 42,31
        beast1frame17 174,160, 42,31
        beast0_pos     20,  0,  1,1
        beast1_pos    258,  0,  1,1
    ]
    image (name: tree filename: "a2/program/TREE.SPK" filetype: a2spk) [
        moongate  60, 0, 21,27   ; Blue sky, for the moongate rising
        items     48,43, 28,27   ; The picture behind the moongate
        gate_pos  48,43,  1, 1
    ]
    image (name: portal filename: "a2/program/PRTL.SPK" filetype: a2spk)
    image (name: outside filename: "a2/program/LOOK.SPK" filetype: a2spk)
    image (name: inside filename: "a2/program/FAIR.SPK" filetype: a2spk)
    image (name: wagon filename: "a2/program/WAGN.SPK" filetype: a2spk)
    image (name: gypsy filename: "a2/program/GYPS.SPK" filetype: a2spk)
    image (name: abacus filename: "a2/program/TABL.SPK" filetype: a2spk) [
        bead_pos  132,17,   7,16
        card_pos   34,16, 182, 1
    ]
    ; The bead shapes from NEWGAME drawn on the abacus, one for each column.
    image (name: beads filename: "a2/program/NEWGAME#706,38+TABL.SPK"
           filetype: a2beads) [
        whitebead 0, 0, 56,14
        blackbead 0,14, 56,14
    ]
    ; Positions of the dungeon view walls, doors, holes & ladders.
    image (name: dung_apple2 filename: "a2/underworld/DNGD#c9d,74"
           filetype: a2dngd)
    image (name: cards1 filename: "a2/program/CRDS" filetype: a2cards) [
        honestycard        0,0, 70,112
        compassioncard    70,0, 70,112
    ]
    image (name: cards2 filename: "a2/program/CRDS" filetype: a2cards) [
        valorcard        140,0, 70,112
        justicecard      210,0, 70,112
    ]
    image (name: cards3 filename: "a2/program/CRDS" filetype: a2cards) [
        sacrificecard    280,0, 70,112
        honorcard        350,0, 70,112
    ]
    image (name: cards4 filename: "a2/program/CRDS" filetype: a2cards) [
        spiritualitycard 420,0, 70,112
        humilitycard     490,0, 70,112
    ]
]

; The flag & campfire effects of the DOS tiles do not fit the Apple tiles.
tileanim: [
    scroll:     [scroll 0]
    scroll_pool:[scroll 1,0,0,2]    ; 2=tile_shallows
    frame:      [random 50 frame]
    slow_frame: [random  5 frame]
    cityflag:   [random 50 invert 0,0,0,0]
    lcbflag:    [random 50 invert 0,0,0,0]
    castleflag: [random 50 invert 0,0,0,0]
    shipflag:   [random 50 invert 0,0,0,0]
    pirateflag: [random 50 invert 0,0,0,0]
    campfire:   [random 50 invert 0,0,0,0]
    phantom_flicker: [frame random 50 scroll 1]
]
