: foo ( x -- )
  1 ;EQ
  2 ;EQ
  3 ;EQ
  drop no ;

: main
  1 foo     "foo1" ASSERT
  2 foo     "foo2" ASSERT
  3 foo     "foo3" ASSERT
  4 foo not "foo4" ASSERT
  ;