graphics: [
    texture: atlas 64,4096 [
        tiles    0, 0
        hud     17, 0
        charset 17,33
    ]
    ; The map graphics are in the order of the DOS tiles.
    image (name: tiles filename: "sms/7c83,10+18000,8000" filetype: smstiles tiles: 256 fixup: blackTransparencyHack) [
        at 0,0 size 12,12
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
        at 0,840 tile_fire_phantom 2
    ]
    image (name: charset filename: "sms/3fb58,300" filetype: smsfont tiles: 256)
    ; The pictures of the story.  The SMS moongate rises at the stones by
    ; the lake.
    image (name: tree filename: "sms/4a8a9,1244+1a180,80+7c83,10" filetype: smstree) [
        moongate   0,152, 12,12
        items     12,152, 12,12
        gate_pos 109, 62,  1, 1
    ]
    image (name: portal filename: "sms/4c000,bee" filetype: smspic)
    image (name: outside filename: "sms/49490,1419" filetype: smspic)
    image (name: inside filename: "sms/465ab,1637" filetype: smspic)
    image (name: wagon filename: "sms/48000,1490" filetype: smspic)
    image (name: gypsy filename: "sms/6af91,e70" filetype: smspic)
    ; The gypsy's abacus: a bead of the chosen (white) and the rejected
    ; (black) virtue on the rung of each question, in the column of the
    ; virtue, as the SMS marks it.  The cards stand at either side.
    image (name: abacus filename: "sms/47be2,152" filetype: smspic) [
        bead_pos  128,24,   8,16
        card_pos   56,24, 160, 1
    ]
    image (name: beads filename: "sms/47be2,152+2e180,439+2e5b9,77f+2ed38,60e+2f346,57d+36180,5eb+3676b,5b5+36d20,687+373a7,556" filetype: smscards) [
        whitebead 0,112, 8,16
        blackbead 8,112, 8,16
    ]
    image (name: cards1 filename: "sms/47be2,152+2e180,439+2e5b9,77f+2ed38,60e+2f346,57d+36180,5eb+3676b,5b5+36d20,687+373a7,556" filetype: smscards) [
        honestycard        0,0, 48,112
        compassioncard    48,0, 48,112
    ]
    image (name: cards2 filename: "sms/47be2,152+2e180,439+2e5b9,77f+2ed38,60e+2f346,57d+36180,5eb+3676b,5b5+36d20,687+373a7,556" filetype: smscards) [
        valorcard         96,0, 48,112
        justicecard      144,0, 48,112
    ]
    image (name: cards3 filename: "sms/47be2,152+2e180,439+2e5b9,77f+2ed38,60e+2f346,57d+36180,5eb+3676b,5b5+36d20,687+373a7,556" filetype: smscards) [
        sacrificecard    192,0, 48,112
        honorcard        240,0, 48,112
    ]
    image (name: cards4 filename: "sms/47be2,152+2e180,439+2e5b9,77f+2ed38,60e+2f346,57d+36180,5eb+3676b,5b5+36d20,687+373a7,556" filetype: smscards) [
        spiritualitycard 288,0, 48,112
        humilitycard     336,0, 48,112
    ]
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
