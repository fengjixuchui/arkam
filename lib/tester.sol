: DIE ( s -- ) "Failed: " epr eprn 1 HALT ;


: ASSERT ( v s -- ) swap [ drop ] ;IF DIE ;
  # Print error with s and die if v is false.


: CHECK ( s q -- )
  # Call q then check tos is true and sp is balanced
  # or die with printing error with s.
  # Quotation q should not remain values on rstack.
  swap >r >r sp r> swap >r ( r: s sp )
  call
  not         IF rdrop                 r> DIE END
  sp r> = not IF "Stack imbalance, " epr r> DIE END
  rdrop
;