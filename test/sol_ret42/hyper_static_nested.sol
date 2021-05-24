: foo ;
: foo
  : bar foo ;
  : bar
    : baz foo ;
    bar ;
  foo ;

: main foo foo:bar foo:bar:baz 42 ;