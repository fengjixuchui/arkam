include: "sarkam.sol"
include: "entity.sol"
include: "basic_sprite.sol"
include: "mgui.sol"


: logo
  const: ox 80
  const: oy 90
  val: x  val: y
  ox x! oy y!
  0xA1 ppu:sprite:i!
  x y ppu:sprite:plot
  x 9 + x!
  4 [ 0xA2 + ppu:sprite:i!
    x y ppu:sprite:plot
    x 6 + x!
  ] for

  3 ppu:color!
  40 [ 8 * ppu:width mod 3 + x!
    x  0  ppu:width x -  ppu:height line
  ] for
;

: main_loop
  ppu:0clear
  mgui:update

  logo

  ppu:switch!
  AGAIN
;


: main
  "ARKAM" emu:title!
  yes emu:show_cursor!
  rand:init
  mgui:init
  basic_sprite:load

  main_loop
;