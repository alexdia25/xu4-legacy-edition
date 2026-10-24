graphics: [
    texture: atlas 64,4096 [
        tiles    0, 0
        hud     17, 0
        charset 17,33
    ]
    image (name: tiles filename: "c64/remaster/mem#b000,2000+mem#e1b0,100+mem#a800,200+mem#f300,100" filetype: c64tiles tiles: 256 fixup: blackTransparencyHack) [
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
    image (name: charset filename: "c64/remaster/mem#e400,400" filetype: c64font tiles: 256)
    ; Intro screens.
    image (name: title filename: "c64/remaster/mem#4000,1e00+mem#9200,400" filetype: c64hires fixup: c64intro) [
        options_top  0, 32, 320,128
        options_btm  0,112, 320, 80
        t_and      148,17,  24, 4
        t_bar       82,31, 150, 1
        t_origin    84,21, 148, 9
        t_present  132,33,  54, 5
        t_title     56,33, 204,47
        t_subtitle  36,80, 242,14
    ]
    image (name: beasties filename: "c64/remaster/mem#7400,1e00" filetype: c64hires) [
        beast0frame00   0,  0, 55,31
        beast0frame01   0, 32, 55,31
        beast0frame02   0, 64, 55,31
        beast0frame03   0, 96, 55,31
        beast0frame04   0,128, 55,31
        beast0frame05   0,160, 55,31
        beast0frame06  56,  0, 55,31
        beast0frame07  56, 32, 55,31
        beast0frame08  56, 64, 55,31
        beast0frame09  56, 96, 55,31
        beast0frame10  56,128, 55,31
        beast0frame11  56,160, 55,31
        beast0frame12 112,  0, 55,31
        beast0frame13 112, 32, 55,31
        beast0frame14 112, 64, 55,31
        beast0frame15 112, 96, 55,31
        beast0frame16 112,128, 55,31
        beast0frame17 112,160, 55,31
        beast1frame00 176,  0, 48,31
        beast1frame01 176, 32, 48,31
        beast1frame02 176, 64, 48,31
        beast1frame03 176, 96, 48,31
        beast1frame04 176,128, 48,31
        beast1frame05 176,160, 48,31
        beast1frame06 224,  0, 48,31
        beast1frame07 224, 32, 48,31
        beast1frame08 224, 64, 48,31
        beast1frame09 224, 96, 48,31
        beast1frame10 224,128, 48,31
        beast1frame11 224,160, 48,31
        beast1frame12 272,  0, 48,31
        beast1frame13 272, 32, 48,31
        beast1frame14 272, 64, 48,31
        beast1frame15 272, 96, 48,31
        beast1frame16 272,128, 48,31
        beast1frame17 272,160, 48,31
        beast0_pos     16,  0,  1,1
        beast1_pos    248,  0,  1,1
    ]
    image (name: tree filename: "c64/remaster/145" filetype: c64tree) [
        moongate   0,152, 24,27     ; The moongate (made by the loader)
        items     24,152, 24,27     ; The ground it rises from
        gate_pos  78, 51,  1, 1
    ]
    image (name: portal filename: "c64/remaster/146" filetype: c64multi)
    image (name: outside filename: "c64/remaster/147" filetype: c64multi)
    image (name: inside filename: "c64/remaster/148" filetype: c64multi)
    image (name: wagon filename: "c64/remaster/149" filetype: c64multi)
    image (name: gypsy filename: "c64/remaster/14a" filetype: c64multi)
    image (name: abacus filename: "c64/remaster/14b" filetype: c64multi) [
        bead_pos  128,17,   8,16
        card_pos   16,16, 208, 1
    ]
    ; The bead shapes from the character creation code, in the colors of
    ; each abacus column.
    image (name: beads filename: "c64/remaster/151#70c,1c+14b"
           filetype: c64beads) [
        whitebead 0, 0, 64,14
        blackbead 0,14, 64,14
    ]
    image (name: cards1 filename: "c64/remaster/152+19f" filetype: c64rcards) [
        honestycard        0,0, 80,112
        compassioncard    80,0, 80,112
    ]
    image (name: cards2 filename: "c64/remaster/152+19f" filetype: c64rcards) [
        valorcard        160,0, 80,112
        justicecard      240,0, 80,112
    ]
    image (name: cards3 filename: "c64/remaster/152+19f" filetype: c64rcards) [
        sacrificecard    320,0, 80,112
        honorcard        400,0, 80,112
    ]
    image (name: cards4 filename: "c64/remaster/152+19f" filetype: c64rcards) [
        spiritualitycard 480,0, 80,112
        humilitycard     560,0, 80,112
    ]
]

; The flags of the Remastered tiles have colors that inverting does not
; match, so they are still, as is the campfire.
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
