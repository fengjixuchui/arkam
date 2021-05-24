: ppu
  : query 3 io ;
  ( ----- palette ----- )
  : palette_color! 0 query ; ( color i -- )
  : color!         1 query ; ( i -- )
  : palette!       2 query ; ( i -- )
  : palette        3 query ; ( -- i )
  ( ----- draw ----- )
  : clear!   10 query ; ( -- )
  : plot     11 query ; ( x y -- )
  : ploti    12 query ; ( i -- )
  : switch!  13 query ; ( -- )
  : trans!   14 query ; ( addr -- )
  : copy!    15 query ; ( -- )
  : width    16 query ; ( -- w )
  : height   17 query ; ( -- h )
  ( ----- sprite ----- )
  : sprite
    const: width 8
    const: size  64
    : i!   20 query ; ( i -- )
    : load 21 query ; ( addr -- )
    : plot 22 query ; ( x y -- )
    : bulk_load ( addr n -- )
      [ ( addr i -- addr )
        dup i! size * over + load
      ] for drop ;
    : load_datafile ( addr -- )
      dup [ 4 + ] [ @ size / ] biq bulk_load ;
  ;
  ( ----- utils ----- )
  : 0clear
    0 palette!
    0 color!
    clear!
  ;
;
