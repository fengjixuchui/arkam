include: "sarkam.sol"
include: "entity.sol"
include: "basic_sprite.sol"
include: "mgui.sol"


: main_loop
  ppu:0clear
  mgui:update


  ppu:switch!
  AGAIN
;


: main
  "scratch" emu:title!
  yes emu:show_cursor!
  rand:init
  mgui:init
  basic_sprite:load

  main_loop
;