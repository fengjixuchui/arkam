: foo ( x -- )
  1 [ ok ] ;CASE
  2 [ no ] ;CASE
  3 [ ok ] ;CASE
  drop no ;

: main
  1 foo     "foo1" ASSERT
  2 foo not "foo2" ASSERT
  3 foo     "foo3" ASSERT
  4 foo not "foo4" ASSERT 
  ;