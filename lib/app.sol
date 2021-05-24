( ===== Application Process ===== )

: app
  : query 12 io ;
  : argc    0 query ; # -- n
  : get_arg 1 query ; # addr i len -- ?

  ( ----- shorthands ----- )
  : get_arg! get_arg IF RET END "Can't get arg!" panic ;
;