; The CGA graphics of the DOS game, read from its own files (see
; src/imageloader_cga.cpp).  The rest comes from the Ultima-IV module.
graphics: [
    texture: atlas 64,4096 [
        tiles    0, 0
        hud     17, 0
        charset 17,33
    ]
    image (name: tiles filename: "u4/shapes.cga" filetype: cgatiles tiles: 256 fixup: blackTransparencyHack) [
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
    image (name: charset filename: "u4/charset.cga" filetype: cgafont tiles: 256)
    image (name: borders filename: "u4/start.pic" filetype: cgapic)
    image (name: title filename: "u4/title.pic" filetype: cgapic fixup: intro) [
        options_top 0, 32, 320,128
        options_btm 0,112, 320, 80
    ]
    image (name: tree filename: "u4/tree.pic" filetype: cgapic) [
        moongate  0,152, 24,24
        items    24,152, 24,24
        gate_pos  72,68, 1,1
    ]
    image (name: portal filename: "u4/portal.pic" filetype: cgapic)
    image (name: outside filename: "u4/outside.pic" filetype: cgapic)
    image (name: inside filename: "u4/inside.pic" filetype: cgapic)
    image (name: wagon filename: "u4/wagon.pic" filetype: cgapic)
    image (name: gypsy filename: "u4/gypsy.pic" filetype: cgapic)
    image (name: abacus filename: "u4/abacus.pic" filetype: cgapic fixup: abacus) [
        whitebead  8,187, 8,12
        blackbead 24,187, 8,12
        bead_pos  128,24, 9,15
        card_pos   12,12, 206,1
    ]
    image (name: cards1 filename: "u4/honcom.pic" filetype: cgapic) [
        honestycard     12,12, 90,124
        compassioncard 218,12, 90,124
    ]
    image (name: cards2 filename: "u4/valjus.pic" filetype: cgapic) [
        valorcard    12,12, 90,124
        justicecard 218,12, 90,124
    ]
    image (name: cards3 filename: "u4/sachonor.pic" filetype: cgapic) [
        sacrificecard   12,12, 90,124
        honorcard      218,12, 90,124
    ]
    image (name: cards4 filename: "u4/spirhum.pic" filetype: cgapic) [
        spiritualitycard   12,12, 90,124
        humilitycard      218,12, 90,124
    ]
    image (name: beasties filename: "u4/animate.pic" filetype: cgapic) [
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
    ]
    image (name: key filename: "u4/key7.pic" filetype: cgapic)
    image (name: honesty filename: "u4/honesty.pic" filetype: cgapic fixup: transparent0)
    image (name: compassn filename: "u4/compassn.pic" filetype: cgapic fixup: transparent0)
    image (name: valor filename: "u4/valor.pic" filetype: cgapic fixup: transparent0)
    image (name: justice filename: "u4/justice.pic" filetype: cgapic fixup: transparent0)
    image (name: sacrific filename: "u4/sacrific.pic" filetype: cgapic fixup: transparent0)
    image (name: honor filename: "u4/honor.pic" filetype: cgapic fixup: transparent0)
    image (name: spirit filename: "u4/spirit.pic" filetype: cgapic fixup: transparent0)
    image (name: humility filename: "u4/humility.pic" filetype: cgapic fixup: transparent0)
    image (name: truth filename: "u4/truth.pic" filetype: cgapic fixup: transparent0)
    image (name: love filename: "u4/love.pic" filetype: cgapic fixup: transparent0)
    image (name: courage filename: "u4/courage.pic" filetype: cgapic fixup: transparent0)
    image (name: stoncrcl filename: "u4/stoncrcl.pic" filetype: cgapic)
    image (name: infinity filename: "u4/rune_5.pic" filetype: cgapic)
    image (name: rune1 filename: "u4/rune_1.pic" filetype: cgapic)
    image (name: rune2 filename: "u4/rune_2.pic" filetype: cgapic)
    image (name: rune3 filename: "u4/rune_0.pic" filetype: cgapic)
    image (name: rune4 filename: "u4/rune_1.pic" filetype: cgapic)
    image (name: rune5 filename: "u4/rune_2.pic" filetype: cgapic)
    image (name: rune6 filename: "u4/rune_1.pic" filetype: cgapic)
    image (name: rune7 filename: "u4/rune_3.pic" filetype: cgapic)
    image (name: rune8 filename: "u4/rune_4.pic" filetype: cgapic)
]

tileanim: [
    scroll:     [scroll 0]
    scroll_pool:[scroll 1,0,0,2]    ; 2=tile_shallows
    frame:      [random 50 frame]
    slow_frame: [random  5 frame]
    cityflag:   [random 50 invert 5,2,4,4]
    lcbflag:    [random 50 invert 4,0,4,4]
    castleflag: [random 50 invert 4,0,4,4]
    shipflag: [
        random 50
        invert 6,1,3,4 context-frame 0
        invert 0,0,0,0 context-frame 1
        invert 7,1,3,4 context-frame 2
        invert 0,0,0,0 context-frame 3
    ]
    pirateflag: [random 50 invert 0,1,16,2]
    campfire: [
        pixel_color 3,5,7,11 [
              0, 0, 0
            255,85,255
        ]
        pixel_color 4,8,5,6 [
            255, 85,255
            255,255,255
        ]
    ]
    phantom_flicker: [frame random 50 scroll 1]
]
