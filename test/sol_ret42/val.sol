: inc! ( a -- ) dup @ 1 + swap ! ;

: foo
  val: x
  val: one
  40 x!
  &one inc! &one inc!
  one x + x!
  x
;

: main foo ;
  