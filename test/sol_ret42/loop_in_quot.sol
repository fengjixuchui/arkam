: main
  : call ( q -- ) >r ;
  : loop-quot ( -- q )
    [ ( n -- ) dup 10 > IF 42 RET END 1 + AGAIN ] ;
  1 loop-quot call HALT
;