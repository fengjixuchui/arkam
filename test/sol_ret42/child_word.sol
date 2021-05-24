: module
  : forty 40 ;
  : two   2 ;
  1 HALT # do not reach here!
;

: main
  module:forty
  module:two
  +
  HALT
;