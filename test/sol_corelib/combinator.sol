: test/dip
  "dip 1" [ 1 2 [ inc ] dip + ( => 2 2 + ) 4 = ] CHECK
;


: test/sip
  "sip 1" [ 1 [ inc ] sip + ( => 2 1 + ) 3 = ] CHECK
;


: test/biq
  "biq 1" [ 1 [ 1 + ] [ 2 + ] biq + ( => 2 3 +     ) 5 = ] CHECK
  "biq 2" [ 1 [ 1   ] [ + + ] biq   ( => 1 1 1 + + ) 3 = ] CHECK
;


: test/bia
  "bia 1" [ 1 2 [ inc ] bia + ( => 2 3 + )     5 = ] CHECK
  "bia 2" [ 1 2 3 [ + ] bia   ( => 1 2 + 3 + ) 6 = ] CHECK
;


: test/bi*
  "bi* 1" [ 1 2 [ 1 + ] [ 2 + ] bi* + ( => 2 4 + )     6 = ] CHECK
  "bi* 2" [ 2 3 [ 1   ] [ + + ] bi*   ( => 2 1 3 + + ) 6 = ] CHECK 
;


: test/triq
  "triq 1" [ 1 [ 1 + ] [ 2 + ] [ 3 + ] triq + + ( => 2 3 4 + + ) 9 = ] CHECK
;


: test/tria
  "tria 1" [ 1 2 3 [ inc ] tria + + ( => 2 3 4 + + ) 9 = ] CHECK
;


: test/tri*
  "tri* 1" [ 1 2 3 [ inc ] [ inc ] [ inc ] tri* + + ( => 2 3 4 + + ) 9 = ] CHECK
;


: main
  test/dip
  test/sip
  
  test/biq
  test/bia
  test/bi*
  
  test/triq
  test/tria
  test/tri*
;
