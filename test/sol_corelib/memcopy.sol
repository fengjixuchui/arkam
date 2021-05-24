: test
  val: buf0
  val: buf1
  val: buf2
  8 allot buf0!
  8 allot buf1!
  8 allot buf2!
  8 [ buf0 + 42 swap b! ] for

  ( check precondition )
  "memcopy precond1" [ buf0 buf1 < ] CHECK
  "memcopy precond2" [ buf1 buf2 < ] CHECK

  ( check copy )
  "memcopy copy 1" [ buf0 buf1 8 memcopy ok ] CHECK
  "memcopy copy 2" [ buf1 b@ 42 = ] CHECK

  ( check overrun )
  "memcopy overrun1" [ buf2 b@ 42 != ] CHECK
;

: main test ;