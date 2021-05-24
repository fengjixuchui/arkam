: test
  "2drop" [ ok 0 0 2drop ] CHECK
  
  "over" [ 1 2 over + + 4 = ] CHECK

  "2dup" [ 1 2 2dup + + + 6 = ] CHECK

  "nip" [ ng ok nip ] CHECK

  "pick" [
    1 2 3
    0 pick  3 = "pick 0" ASSERT
    1 pick  2 = "pick 1" ASSERT
    2 pick  1 = "pick 2" ASSERT
    2drop drop ok
  ] CHECK
;

: main "all" [ test ok ] CHECK ;