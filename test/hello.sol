: putc 0 1 io ;
: exit 0 HALT ;

: puts # addr --
  dup b@
  dup 0 = IF drop drop RET END
  putc 1 + AGAIN ;

: hello "hello\n" puts ;

: main hello exit ;