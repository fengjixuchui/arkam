: check ( a b s ) >r swap align = r> ASSERT ;
  # check aligned a equals b


: main
  0 0 "0 => 0" check
  1 4 "1 => 4" check
  3 4 "3 => 4" check
  4 4 "4 => 4" check
  5 8 "5 => 8" check
  ;