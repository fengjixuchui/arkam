: test
  here dup
  31 allot       = "addr"  ASSERT
  here swap - 32 = "align" ASSERT
;

: main test ;