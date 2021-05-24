include: "sarkam.sol"
include: "basic_sprite.sol"
include: "mgui.sol"



: sprite_editor
  const: max_sprites 256
  val: filename
  val: bytes
  const: w     8    ( width )
  const: dots  8
  const: size  64   ( 8 * 8 )
  val: data
  const: i0 0
  const: i1 1
  const: i2 2
  const: i3 3
  val: si
  val: ox
  val: oy
  const: pad 1  const: between 18 ( editor/sprite-list )
  val: grid
  val: i  val: x  val: y  val: c
  val: mx  val: my  val: mp  ( mouse )
  val: px  val: py

  : at ( i -- addr ) size * data + ;
  : target ( -- addr ) si at ;
  : fill ( i c ) c! at 64 [ over + c swap b! ] for ;

  : set_sp
    0 [ i0 ppu:sprite:i! ] ;CASE
    1 [ i1 ppu:sprite:i! ] ;CASE
    2 [ i2 ppu:sprite:i! ] ;CASE
    3 [ i3 ppu:sprite:i! ] ;CASE
    ? drop "UNKNOWN SP" panic
  ;
  : dot ( x y ) 2 ppu:color! ppu:plot ;

  ( ----- load/save ----- )
  val: file_id
  : fname filename dup IF RET END "No filename given!" panic ;
  : close_file file_id file:close! ;
  : open_load fname "rb" file:open! file_id! ;
  : load_file open_load data bytes file_id file:read! close_file ;
  : load      load_file "loaded!" prn ;
  : open_save fname "wb+" file:open! file_id! ;
  : save_file open_save data bytes file_id file:write! close_file ;
  : save      save_file "saved!" prn ;
  : draw_fname  2 2 fname put_text ;

  : showcase
    val: width  val: height ( full )
    val: w      val: h      ( list )
    val: left   val: right  val: top  val: bottom ( list )
    const: scroll_margin 8
    const: scroll_width  8
    const: state_margin  8
    const: state_height  8
    const: left_margin   20
    val: baseline
    const: sprites/line 8
    const: lines/page   8
    val:   sprites/page
    val:   max_lines
    const: min_baseline 0
    val:   max_baseline
    : base_index   sprites/line baseline * ;
    : sprite_index ( x y -- i ) sprites/line * + base_index + ;
    : i>si base_index + ;
    : base_move ( n )
      dup min_baseline < IF drop min_baseline baseline! RET END
      dup max_baseline > IF drop max_baseline baseline! RET END
      baseline! ;
    : base_find si sprites/line / 4 - base_move ;
    : base_up   baseline swap - base_move ;
    : base_down baseline +      base_move ;
    : calc_size
      dots 1 + sprites/line * w!
      dots 1 + sprites/line * h!
      w scroll_margin + scroll_width + width!
      h state_margin + state_height + height! ;
    : calc_pos
      left w + right!
      top  h + bottom! ;
    : init
      0 baseline!
      sprites/line lines/page * sprites/page!
      max_sprites sprites/line / max_lines!
      max_lines lines/page - max_baseline!
      0 right scroll_margin + top         138 [ drop lines/page base_up   ] sprite_button:create
      0 right scroll_margin + top    9  + 142 [ drop 1          base_up   ] sprite_button:create
      0 right scroll_margin + top    36 + 144 [ drop            base_find ] sprite_button:create
      0 right scroll_margin + bottom 17 - 143 [ drop 1          base_down ] sprite_button:create
      0 right scroll_margin + bottom 8  - 139 [ drop lines/page base_down ] sprite_button:create
    ;
    : handle_mouse
      val: pressed
      : hover?
        mx left   <  IF no RET END
        mx right  >= IF no RET END
        my top    <  IF no RET END
        my bottom < ;
      mp 0 = IF no pressed! END
      hover? not IF RET END
      mx left - sprites/line pad + / x!
      my top  - sprites/line pad + / y!
      x y sprite_index i!
      mp IF i si! yes pressed! END
    ;
    : draw
      si left bottom 8 + put_hex
      ( ----- all sprites ----- )
      9 [ y!
        9 y * top + py!
        9 [ x!
          9 x * left + px!
          ( draw dot )
          px 1 - py 1 - dot
          x 8 = IF RET END
          y 8 = IF RET END
          ( draw sprite )
          x y sprite_index ppu:sprite:i!
          px py ppu:sprite:plot
          ( draw select )
          x y sprite_index si != IF RET END
          3 ppu:color!
          px 1 - py 1 - 10 10 rect
        ] for
      ] for ;
  ;

  : editor
    val: width  val: height
    val: left  val: right  val: top  val: bottom ( editor )
    val: col_bottom  val: full_height
    val: cx  val: cy  ( color selector )
    const: sprite/line 8
    const: col_margin 8
    const: col_height 8
    val: pen_color
    : calc_size
      dots 1 + sprite/line * width!
      dots 1 + sprite/line * height!
      height col_margin + col_height + full_height! ;
    : calc_pos
      left width  + right!
      top  height + bottom!
      left cx!
      bottom col_margin + cy!
      cy col_height + col_bottom! ;
    : rotate_left ( i -- )
      # not used now
      val: first  val: i  val: a  val: p
      i! i at a!
      8 [ y!
        8 [ x!
          8 y * x + a + p!
          x 7 = IF first p b! RET END
          x 0 = IF p b@ first! END
          p 1 + b@ p b!
        ] for
      ] for ;
    : init
      3 pen_color!
      1 ( pen color ) 9 0 * cx + cy 1 [ pen_color! ] sprite_button:create
      2 ( pen color ) 9 1 * cx + cy 2 [ pen_color! ] sprite_button:create
      3 ( pen color ) 9 2 * cx + cy 3 [ pen_color! ] sprite_button:create ;
    : handle_mouse
      val: pressed  val: draw_color
      : hover?
        mx left   <  IF no RET END
        mx right  >= IF no RET END
        my top    <  IF no RET END
        my bottom < ;
      : toggle_color target i + b@ IF 0 ELSE pen_color END draw_color! ;
      : paint draw_color target i + b! ;
      mp 0 = IF no pressed! END
      hover? not IF RET END
      mx left - sprite/line pad + / x!
      my top  - sprite/line pad + / y!
      y sprite/line * x + i!
      mp IF
        pressed not IF
          toggle_color
          yes pressed!
        END
        paint
      END
    ;
    : draw
      ( ----- color selector ----- )
      3 ppu:color!
      9 pen_color 1 - * cx +     cy 9 +
      9 pen_color 1 - * cx + 7 + cy 9 + line
      ( ----- target ----- )
      9 [ y!
        9 y * top + py!
        9 [ x!
          9 x * left + px!
          ( draw dot )
          px 1 - py 1 - dot
          x 8 = IF RET END
          y 8 = IF RET END
          ( draw sprite )
          sprite/line y * x + target + b@ set_sp
          px py ppu:sprite:plot
        ] for
      ] for
    ;
  ;

  : menu
    val: left  val: top
    const: top_margin 20
    const: pad 1
    const: height 8
    : calc_size ;
    : calc_pos ;
    : init
      0 left top "save" [ drop save ] text_button:create
    ;
  ;

  ( ----- init ----- )
  : calc_size editor:calc_size showcase:calc_size menu:calc_size ;
  : calc_pos
    editor:width showcase:left_margin + showcase:width + ppu:width swap - 2 / ox!
    editor:full_height menu:top_margin + menu:height + ppu:height swap - 2 / oy!
    ( editor )
    ox editor:left!
    oy editor:top!
    editor:calc_pos
    ( showcase )
    editor:right showcase:left_margin + showcase:left!
    oy showcase:top!
    showcase:calc_pos
    ( menu )
    ox menu:left!
    editor:col_bottom menu:top_margin + menu:top!
    menu:calc_pos
  ;
  : load_sprites
    fname file:exists? IF
      bytes allot dup data! max_sprites ppu:sprite:bulk_load
      load
    ELSE
      basic_sprite:data data!
    END
  ;

  : init
    w w * max_sprites * bytes!
    load_sprites
    i1 1 fill
    i2 2 fill
    i3 3 fill
    4 si!
    w pad + w * grid!
    ( --- size and pos--- )
    calc_size calc_pos
    editor:init
    showcase:init
    menu:init
  ;
  ( ----- draw ----- )
  : handle_mouse
    mouse:x  mx!
    mouse:y  my!
    mouse:lp mp!
    editor:handle_mouse
    showcase:handle_mouse
  ;
  : draw
    handle_mouse
    editor:draw
    showcase:draw
    draw_fname
  ;
;


: main_loop
  ppu:0clear
  mgui:update

  sprite_editor:draw

  ppu:switch!
  AGAIN
;


: main
  const: fnamelen 512
  val: fname
  "sprited" emu:title!
  yes emu:show_cursor!
  rand:init
  mgui:init

  app:argc 0 > IF
    fnamelen allot fname!
    fname 0 fnamelen app:get_arg!
    "file: " pr fname prn
    fname sprite_editor:filename!
  ELSE
    "Usage: sarkam sprited.ark FILENAME" panic
  END

  basic_sprite:load
  sprite_editor:init

  main_loop
;