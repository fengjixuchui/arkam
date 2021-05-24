: fm
  : query 4 io ;

  : voice!     0 query ; # i --
  : operator!  1 query ; # i --
  : play       2 query ; # freq --
  : stop       3 query ;
  : param!     4 query ;
  : algo!      5 query ; # i --

  ( param )
  : attack!    0 param! ; # v --
  : decay!     1 param! ; # v --
  : sustain!   2 param! ; # v --
  : release!   3 param! ; # v --
  : mod_ratio! 4 param! ; # v --
  : fb_ratio!  5 param! ; # v --
  : wave!      6 param! ; # v --
  : ampfreq!   7 param! ; # v --
  : fm_level!  8 param! ; # v --

  const: C3  261
  const: D3  293
  const: E3  329
  const: F3  349
  const: G3  392
  const: A3  440
  const: B3  493
  const: C4  523
  const: Db3 277
  const: Eb3 311
  const: Gb3 369
  const: Ab3 415
  const: Bb3 466
;