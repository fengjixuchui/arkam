: loop # n --
  dup 41 > IF HALT END
  1 + AGAIN
;

: main 1 loop ;