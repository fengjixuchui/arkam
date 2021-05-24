: foo
  : run 0 HALT ;
  ;

: run 0 ;
: foo
  : bar ;
  ;

: main foo:run ;