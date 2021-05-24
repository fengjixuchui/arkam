( ===== ECS: Entity Component System ===== )

: ecs
  : size   @              ; # es -- n
  : size!  !              ; # n es --
  : next   1 cells + @    ; # es -- n
  : next!  1 cells + !    ; # n es --
  : alive? 2 cells + + b@ ; # id es -- ?
  : dead?  alive? not     ; # id es -- ?
  : alive! 2 cells + + yes swap b! ; # id es --
  : dead!  2 cells + + no  swap b! ; # id es --
  ( ----- create entity table ----- )
  : entities ( size -- addr )
    val: es  val: s
    dup s! 2 cells + allot es!
    s es size!
    0 es next!
    es ;
  ( ----- entity operation -----)
  : new ( es -- id not-full | full )
    val: es  val: start  val: nx  val: id  val: max
    : find
      nx dup id! 1 + max mod nx!
      id es dead? IF ( found )
        id es alive!
        nx es next!
        id no RET
      END
      nx start = IF yes RET END ( full )
      AGAIN ;
    es!
    es size max!
    es next dup nx! start!
    find ;
  : new! ( es -- id ) new IF "Full entities!" panic END ;
  : delete ( id es -- ) dead! ;
  : each ( es q -- )
    # iterate each alive entity
    # q: id --
    swap dup size [ swap ( q id es -- q es )
      2dup alive? IF
        >r swap dup >r call r> r>
      ELSE
        swap drop
      END
    ] for 2drop
  ;
  ( ----- component cs ----- )
  # size data...
  : components size dup 1 + cells allot swap over ! ; # es -- addr
  : component_size @ ;
  : component_at 1 cells + swap cells + ; # id cs -- addr
  : get component_at @ ; # id cs -- v
  : set component_at ! ; # v id cs --
  : find ( v cs -- id yes | no )
    val: cs  val: v  val: id
    : loop
      id 0 < IF no RET END
      id cs get v = IF id yes RET END
      id 1 - id! AGAIN
    ;
    cs! v! cs component_size 1 - id! loop
  ;
  : find! ( v cs -- id ) find IF RET END "entity not found!" panic ;
;
