: call >r ;
: bar 42 HALT ;
: foo [ [ bar ] call ] ; # nest!
: main foo call ;