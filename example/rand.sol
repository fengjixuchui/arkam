: test:hard
  val: s0
  val: s1
  val: s2
  val: s3
  val: s4
  : inc! dup @ 1 + swap ! ;
  : reset 0 s0! 0 s1! 0 s2! 0 s3! 0 s4! ;
  : write ( n -- )
    0 [ &s0 inc! ] ;CASE
    1 [ &s1 inc! ] ;CASE
    2 [ &s2 inc! ] ;CASE
    3 [ &s3 inc! ] ;CASE
    4 [ &s4 inc! ] ;CASE
  ;
  : stars ( a -- ) ? 100 / [ "*" pr ] for cr ;
  : show s0 stars s1 stars s2 stars s3 stars s4 stars ;
  : go reset rand:init 10000 [ 5 rand write ] for show ;

  cr "===== hard =====" prn
  cr "seed time (10000)" prn go
;


: test:soft
  : go xorshift ? drop cr ;

  cr "===== soft =====" prn
  
  cr "seed 1" prn
  1 xorshift:s! go go go go
  
  cr "seed 2" prn
  2 xorshift:s! go go go go
;


: main
  test:hard cr
  ( test:soft cr )
;