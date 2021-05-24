: basic_sprite
  datafile: spr "basic.spr"
  const: width       8
  const: sprite_size 64 ( 8x8 )
  val: sprites
  : data spr 1 cells + ;
  val: size
  : size &size [ spr @ ppu:sprite:size / ] init! ;
  : load data size ppu:sprite:bulk_load ;
;