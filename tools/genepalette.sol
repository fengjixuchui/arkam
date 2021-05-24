include: "mgui.sol"
include: "entity.sol"
include: "color8.sol"


: genepalette
  const: max_genes   8
  const: op/gene     4
  const: op_len      4 ( op, p1, p2, p3  4 bytes )
  const: gene_bytes 22 ( bytes - min/max for rgb 6bytes, op 16 bytes )
  : genes_bytes gene_bytes max_genes * ;
  val: genes    ( entities )
  val: genes1   ( current )
  val: genes2   ( next )
  val: palettes ( palette numbers )
  val: hs  val: ss  val: vs  ( hsv for entities )
  val: rgb1s  val: rgb2s  val: rgb3s  val: rgb4s  ( rgb8 for entities )
  val: generation
  ( ----- ring arithmatics ----- )
  : add + 256 mod ; # a b -- n
  : mul * 256 mod ; # a b -- n
  : rand_minmax ( a b -- n )
    2dup max >r min r>         ( min max )
    dup 0 = IF 2drop 0 RET END ( max=0 => 0 )
    over - dup 0 = IF 2drop RET END ( diff=0 => min )
    rand + ;
  ( ----- accessors ----- )
  : gene_addr swap gene_bytes * + ; # id addr -- addr
  : gene genes1 gene_addr ; # id -- addr
  : h hs ecs:get ;  : h! hs ecs:set ;
  : s ss ecs:get ;  : s! ss ecs:set ;
  : v vs ecs:get ;  : v! vs ecs:set ;
  : rgb1 rgb1s ecs:get ;    : rgb1! rgb1s ecs:set ;
  : rgb2 rgb2s ecs:get ;    : rgb2! rgb2s ecs:set ;
  : rgb3 rgb3s ecs:get ;    : rgb3! rgb3s ecs:set ;
  : rgb4 rgb4s ecs:get ;    : rgb4! rgb4s ecs:set ;
  : hmin gene     b@ ;   : hmin! gene     b! ;
  : hmax gene 1 + b@ ;   : hmax! gene 1 + b! ;
  : smin gene 2 + b@ ;   : smin! gene 2 + b! ;
  : smax gene 3 + b@ ;   : smax! gene 3 + b! ;
  : vmin gene 4 + b@ ;   : vmin! gene 4 + b! ;
  : vmax gene 5 + b@ ;   : vmax! gene 5 + b! ;
  : randh &hmin &hmax biq rand_minmax ; # id -- h
  : rands &smin &smax biq rand_minmax ; # id -- s
  : randv &vmin &vmax biq rand_minmax ; # id -- v
  ( op: i p1 p2 p3 )
  : op_start gene 6 + ; # id -- addr
  : op_addr  op_len * swap op_start + ; # id i -- addr
  : op1 gene  6 + ;
  : op2 gene 10 + ;
  : op3 gene 14 + ;
  : op4 gene 18 + ;
  : op_i      b@ ;  : op_i!      b! ;
  : op_p1 1 + b@ ;  : op_p1! 1 + b! ;
  : op_p2 2 + b@ ;  : op_p2! 2 + b! ;
  : op_p3 3 + b@ ;  : op_p3! 3 + b! ;
  ( ----- genes data operations ----- )
  : swap_genes genes1 genes2 genes1! genes2! ;
  : copy_genes ( i2 i1 -- )
    # copy gene from genes1(i1) to genes2(i2)
    swap [ gene ] [ genes2 gene_addr ] bi* gene_bytes memcopy
  ;
  ( ----- generation operations ----- )
  const: operations 4
  : setup_hsv ( id -- )
    val: id
    id!
    id randh id h!
    id rands id s!
    id randv id v! ;
  : run_op1 ( op id -- ) # h+op1 s+op2 v+op3
    val: id  val: op
    id! op!
    id h op op_p1 add id h!
    id s op op_p2 add id s!
    id v op op_p3 add id v!
  ;
  : run_op2 ( op id -- ) # h*op1 s*op2 v*op3
    val: id val: op
    id! op!
    id h op op_p1 mul id h!
    id s op op_p2 mul id s!
    id v op op_p3 mul id v!
  ;
  : run_op3 ( op id -- ) # h=h+op1 s=s+op2 v=v
    val: id val: op
    id! op!
    id h op op_p1 add id h!
    id s op op_p2 add id s!
  ;
  : run_op4 ( op id -- ) # h=h s=s+op1 v=v+op2
    val: id val: op
    id! op!
    id s op op_p1 add id s!
    id v op op_p2 add id v!
  ;
  : run_op ( id i -- )
    val: id  val: i  val: op
    i! id!
    id i op_addr op!
    op op_i operations mod
    0 [ op id run_op1 ] ;CASE
    1 [ op id run_op2 ] ;CASE
    2 [ op id run_op3 ] ;CASE
    3 [ op id run_op4 ] ;CASE
    ? "Invalid operator" panic
  ;
  : run_all_op ( id -- ) operations [ over swap run_op ] for drop ;
  ( ----- palette ----- )
  : get_rgb8 ( id -- rgb8 ) &h &s &v triq rgb8:from_hsv rgb8:merge ;
  : set_palette ( c i id -- )
    palettes ecs:get ppu:palette!
    ppu:palette_color!
  ;
  : gen_palette ( id -- )
    val: id
    id!
    id setup_hsv

    id run_all_op
    id get_rgb8 dup id rgb1! 0 id set_palette

    id run_all_op
    id get_rgb8 dup id rgb2! 1 id set_palette

    id run_all_op
    id get_rgb8 dup id rgb3! 2 id set_palette

    id run_all_op
    id get_rgb8 dup id rgb4! 3 id set_palette
  ;
  : reload ( id -- ) gen_palette ;
  ( ----- next generation ----- )
  : ?gene ( id -- id ) dup gene gene_bytes [ over + b@ ?ff space ] for drop ;
  : shake_gene ( id -- )
    const: shake_rate 5 # 1/n
    : shake shake_rate rand IF RET END 256 + 6 rand 3 - add ;
    gene
    gene_bytes [ over + dup b@ shake swap b! ] for drop
  ;
  : mix_gene ( id i2 -- ) # mix gene to id/genes1 from i2/genes2
    const: mix_rate 3 # 1/n
    val: p1  val: p2
    [ gene p1! ] [ genes2 gene_addr p2! ] bi*
    gene_bytes [
      mix_rate rand not IF drop RET END
      [ p2 + b@ ] [ p1 + ] biq b!
    ] for
  ;
  : select_gene ( id -- )
    val: id  val: p2  val: p3
    id!
    id 2 + max_genes mod p2!
    p2 2 + max_genes mod p3!
    "----- next -----" prn
    max_genes [ id copy_genes ] for
    swap_genes
    2 shake_gene
    3 shake_gene
    4 p2 mix_gene
    5 p3 mix_gene
    6 dup shake_gene p2 mix_gene
    7 dup shake_gene p3 mix_gene
    max_genes [ ?gene cr reload ] for
    generation 1 + generation!
  ;
  ( ----- display ----- )
  : display
    val: id
    const: ox 180
    const: oy 8
    const: width 64
    const: spr_start 0
    const: rgb1y 80 ( 8 + 64 )
    const: rgb2y 96
    const: rgb3y 112
    const: rgb4y 128
    : draw_rgb8 ( n x y )
      val: x  val: y
      y! x!
      dup      x 30 + y put_ff ( b )
      8 >> dup x 15 + y put_ff ( g )
      8 >>     x      y put_ff ( r )
    ;
    : draw
      val: y  val: x  val: py
      id palettes ecs:get ppu:palette!
      0 ppu:color!
      ox oy width width fill_rect
      8 [ y!
        y 8 * oy + py!
        8 [ x!
          y 8 * x + 0xa8 + ppu:sprite:i!
          x 8 * ox + py ppu:sprite:plot
        ] for
      ] for
      0 ppu:color!
      ox rgb1y 8 8 fill_rect
      1 ppu:color!
      ox rgb2y 8 8 fill_rect
      2 ppu:color!
      ox rgb3y 8 8 fill_rect
      3 ppu:color!
      ox rgb4y 8 8 fill_rect
      0 ppu:palette!
      id rgb1 ox 10 + rgb1y draw_rgb8
      id rgb2 ox 10 + rgb2y draw_rgb8
      id rgb3 ox 10 + rgb3y draw_rgb8
      id rgb4 ox 10 + rgb4y draw_rgb8
    ;
  ;
  ( ----- draw ----- )
  const: origin_y 8
  const: bar1w 40  const: bar1x 8
  const: bar2w 30  const: bar2x 48
  const: bar3w 20  const: bar3x 78
  const: bar4w 10  const: bar4x 98
  const: btn_show_x     116 ( bar4+8 )
  const: btn_reload_x   132 ( + 16 )
  const: btn_next_x     148 ( + 16 )
  const: btn_show_spr   0xa0
  const: btn_reload_spr 0xa1
  const: btn_next_spr   0x1d
  const: bar_h 8
  const: draw_pad 10
  const: gen_x 8
  const: gen_y 176 ( 192 - 16 )
  : bar_y ( id -- y ) bar_h draw_pad + * origin_y + ;
  : draw_gene ( id -- )
    val: id
    val: y
    : bar
      0 ppu:color!
      bar1x y bar1w bar_h fill_rect
      1 ppu:color!
      bar2x y bar2w bar_h fill_rect
      2 ppu:color!
      bar3x y bar3w bar_h fill_rect
      3 ppu:color!
      bar4x y bar4w bar_h fill_rect
    ;
    id!
    id palettes ecs:get ppu:palette!
    id bar_y y!
    bar
  ;
  : draw
    genes [ draw_gene ] ecs:each
    display:draw
    generation gen_x gen_y put_hex
  ;
  ( ----- initialize ----- )
  : new_components genes ecs:components ;
  : allot_genes
    ( entity ) max_genes ecs:entities genes!
               max_genes [ genes ecs:new! ] times
    ( data ) genes_bytes dup allot genes1! allot genes2!
    ( hsv )
    new_components hs!
    new_components ss!
    new_components vs!
    ( rgb8 )
    new_components rgb1s!
    new_components rgb2s!
    new_components rgb3s!
    new_components rgb4s!
  ;
  : init_genes
    # randomize all genes
    genes_bytes [ genes1 + 256 rand swap b! ] for
  ;
  : init_palettes
    val: id
    genes ecs:components palettes!
    max_genes [ id!
      id inc id palettes ecs:set
      id gen_palette
    ] for
  ;
  : init_gui
    val: id
    max_genes [ id!
      id btn_show_x   id bar_y btn_show_spr   [ display:id!            ] sprite_button:create
      id btn_reload_x id bar_y btn_reload_spr [ dup reload display:id! ] sprite_button:create
      id btn_next_x   id bar_y btn_next_spr   [ select_gene            ] sprite_button:create
    ] for
  ;
  : init
    allot_genes
    init_genes
    init_palettes
    0 display:id!
    init_gui
  ;
;


: main_loop
  ppu:0clear
  mgui:update

  genepalette:draw
  ppu:switch!
  AGAIN
;


: sprites
  datafile: spr "genepalette.spr"
  : load spr ppu:sprite:load_datafile ;
;

: main
  rand:init
  "seed: " pr 1000000 rand ? cr rand:seed!
  # 252700 rand:seed!
  mgui:init
  genepalette:init

  0 ppu:palette!
  0x00eeeeee 0 ppu:palette_color!
  0x00bbbbbb 1 ppu:palette_color!
  0x00999999 2 ppu:palette_color!
  0x00333333 3 ppu:palette_color!

  sprites:load

  main_loop
;