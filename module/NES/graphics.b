; The graphics of the NES release (Ultima: Quest of the Avatar), read from
; the player's own ROM (see src/imageloader_nes.cpp).  The rest comes from
; the Ultima-IV module.
graphics: [
    texture: atlas 64,4096 [
        tiles    0, 0
        hud     17, 0
        charset 17,33
    ]
    ; The map graphics in the order of the DOS tiles, put together from the
    ; NES map tiles & sprites.
    image (name: tiles filename: "nes/0,40000" filetype: nestiles tiles: 256 fixup: blackTransparencyHack) [
        at 0,0 size 16,16
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
    image (name: charset filename: "nes/0,40000" filetype: nesfont tiles: 256)
    ; The title: the NES book cover with its logo is the "Ultima IV" of the
    ; intro; a frame of NES windows goes round the map.
    image (name: title filename: "nes/0,40000" filetype: nestitle fixup: smsintro) [
        options_top  0, 32, 320,128
        options_btm  0,112, 320, 80
        t_signature  0,  0,   1,  1
        t_and        0,  0,   1,  1
        t_bar        0,  0,   1,  1
        t_origin     0,  0,   1,  1
        t_present    0,  0,   1,  1
        t_title     40,  0, 240, 96
        t_subtitle   0,  0,   1,  1
        t_map        0, 96, 320, 96
    ]
    ; A daemon (left) and a dragon (right) turning as NES creatures do.
    image (name: beasties filename: "nes/0,40000" filetype: nesbeasts) [
        at 0,0  size 32,32 beast0frame00 2
        at 0,64 size 32,32 beast1frame00 2
        beast0_pos    4,8, 1,1
        beast1_pos  284,8, 1,1
        beast_steps   0,0, 2,400
    ]
    ; The gypsy's table and abacus.  A bead of the chosen (white) and the
    ; rejected (black) virtue goes on the rung of each question, in the
    ; column of the virtue.
    image (name: abacus filename: "nes/0,40000" filetype: nesgypsy) [
        bead_pos  128,16,   8,16
        card_pos   40,24, 176, 1
    ]
    image (name: beads filename: "nes/0,40000" filetype: nescards) [
        whitebead 512,0, 8,16
        blackbead 520,0, 8,16
    ]
    image (name: cards1 filename: "nes/0,40000" filetype: nescards) [
        honestycard        0,0, 64,104
        compassioncard    64,0, 64,104
    ]
    image (name: cards2 filename: "nes/0,40000" filetype: nescards) [
        valorcard        128,0, 64,104
        justicecard      192,0, 64,104
    ]
    image (name: cards3 filename: "nes/0,40000" filetype: nescards) [
        sacrificecard    256,0, 64,104
        honorcard        320,0, 64,104
    ]
    image (name: cards4 filename: "nes/0,40000" filetype: nescards) [
        spiritualitycard 384,0, 64,104
        humilitycard     448,0, 64,104
    ]
    ; The dungeon view is drawn by the game's own program (see
    ; src/imageloader_nes.cpp).
    image (name: dung_nes filename: "nes/0,40000" filetype: nesdngd)
]

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
