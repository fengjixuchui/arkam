: < swap > ;

: fib
  dup  0 < IF drop 0 RET END
  dup  1 = IF RET END
  dup  2 - RECUR
  swap 1 - RECUR
  +
;

: main
  8 fib  # => 21
  2 *
  HALT
;