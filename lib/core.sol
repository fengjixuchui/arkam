# ===== Sol Core Library =====


( ===== Boolean ===== )

const: ok -1
const: ng  0

const: yes ok
const: no  ng

const: true  ok
const: false ng


: not 0 = IF true RET END false ;


( ===== Stack ===== )

: nip   swap drop ; # a b -- b
: 2dup  over over ; # a b -- a b a b
: 2drop drop drop ; # x x --
: 3drop drop drop drop ; # x x x --


( ===== Compare ===== )

: <= ( a b -- ? ) > bit-not ;
: >= ( a b -- ? ) < bit-not ;


: max ( a b -- a|b ) over over < IF swap END drop ;
: min ( a b -- a|b ) over over > IF swap END drop ;


( ===== Arithmetics ===== )

: /   ( a b -- a/b ) /mod drop ;
: mod ( a b -- a%b ) /mod swap drop ;

: neg ( n -- n ) -1 * ;

: abs dup 0 < IF neg END ;

: inc 1 + ;
: dec 1 - ;


( ===== bit ===== )

: <<  ( n -- n )     bit-lshift ;
: >>  ( n -- n ) neg bit-lshift ;
: >>> ( n -- n ) neg bit-ashift ;


( ===== Memory ===== )

: cells ( n -- n ) 4 * ;
: align ( n -- n ) 3 + 3 bit-not bit-and ;

: inc! ( addr -- ) dup @ 1 + swap ! ;
: dec! ( addr -- ) dup @ 1 - swap ! ;


: memcopy ( src dst len -- )
  : loop ( dst src len -- )
    dup 0 > IF
      1 - >r
      over over b@ swap b!
      1 + swap 1 + swap
      r> AGAIN
    END 3drop ;
  >r swap r> loop ;


( ===== Combinator ===== )

: call ( q -- ) >r ;


: DEFER ( -- ) r> r> swap r> r> ;
  # defers caller's rest process until caller's caller return
  # example:
  #   : foo bar "foo" pr sp ;
  #   : bar "bar1" pr sp DEFER "bar2" pr ;
  # => prints bar1 foo bar2


: dip ( a q -- ... ) swap >r call r> ;
  # escape a, call q, then restore a
  # example:
  #   1 3 [ inc ] dip  => 2 3


: sip ( a q -- ... a ) over >r call r> ;
  # copy & restore a
  # eample:
  #   1 [ inc ] => 2 1


: biq ( a q1 q2 -- aq1 aq2 ) >r over >r call r> ; ( return to quotation )
  # biq - bi quotations application


: bia ( a b q -- aq bq ) swap over >r >r call r> ; ( return to quotation )
  # bia - bi arguments application


: bi* ( a b q1 q2 aq1 bq2 ) >r swap >r call r> ; ( return to quotation )


: triq ( a q1 q2 q3 -- aq1 aq2 aq3 )
  >r >r over r> swap >r ( a q1 q2 | q3 a )
  biq r> ; ( return to quotation )
  # triq - tri quotations application


: tria ( a b c q -- aq bq cq )
  swap over >r >r ( a b q | q c )
  bia r> ; ( return to quotation )
  # tria - tri arguments application


: tri* ( a b c q1 q2 q3 -- aq1 bq2 cq3 )
  >r >r swap r> swap >r ( a b q1 q2 | q3 c )
  bi* r> ; ( return to quotation )



( ===== Iterator ===== )

: times ( n q -- )
  : loop ( q n )
    dup 1 < IF 2drop RET END
    1 - over swap >r >r call r> r> AGAIN ;
  swap loop
;


: for ( n q -- )
  : loop ( q n i )
    2dup <= IF 3drop RET END
    swap over 1 +     # q i n i+1
    >r >r swap dup >r # i q | i+1 n q
    call r> r> r> AGAIN ;
  swap 0 loop
;


( ===== Stdio ===== )

# port 1:stdout 2:stderr
: stdio:ready? -1 1 io ; # -- ?
: putc          0 1 io ; # c --
: getc          1 1 io ; # -- c
: stdio:port    2 1 io ; # -- p
: stdio:port!   3 1 io ; # p --


const: stdout 1
const: stderr 2


: cr    10 putc ;
: space 32 putc ;


: pr ( s -- )
  dup b@ dup 0 = IF 2drop RET END
  putc 1 + AGAIN ;

: prn ( s -- ) pr cr ;


: call/port ( q p -- ) stdio:port >r stdio:port! call r> stdio:port! ;
  # call-with-port
  # call q with port p then restore previous port

: >stdout ( q -- ) stdout call/port ;
: >stderr ( q -- ) stderr call/port ;


: epr  ( s -- ) [ pr  ] >stderr ;
: eprn ( s -- ) [ prn ] >stderr ;


( ===== System ===== )

: sys
  : exit ( code -- ) HALT ;
  : query 0 io ;
  : info
    : size      0 query ;
    : ds_size   2 query ;
    : ds        3 query ;
    : rs_size   4 query ;
    : rs        5 query ;
    : cell_size 6 query ;
    : max_int   7 query ;
    : min_int   8 query ;
    ;
  query
;


( ===== Debug print ===== )

: >ff ( n -- c ) dup 10 < IF 48 ELSE 55 END + ;

: ? ( n -- )
  : pr ( n -- )
    : p    0x30 + putc ;
    : dash 0x2D putc ;
    : sign dup 0 < IF dash neg END ;
    : digits ( n -- )
      10 /mod swap ( % / )
      dup 0 = IF drop p RET END
      RECUR p ;
    sign digits ;
  [ dup pr space ] >stderr
;

: ?ff ( n -- )
  0xff bit-and 16 /mod swap >ff putc >ff putc
;

: ?stack ( -- )
  : loop ( sp -- )
    dup sys:info:rs >= IF drop RET END
    dup @ ? cr drop
    1 cells + AGAIN ;
  [ cr sp 1 cells + loop ] >stderr
;


( ===== Exception ===== )

: die 1 sys:exit ;
: panic ( s -- ) eprn die ;


( ===== Address validation ===== )

: valid
  : dict ( addr -- addr )
    : check IF "invalid address" panic END ;
    dup 0 <            check
    dup sys:info:ds >= check ;
  : ds ( addr -- addr )
    : check IF "invalid stack address" panic END ;
    dup sys:info:ds <  check
    dup sys:info:rs >= check ;
  : rs ( addr -- addr )
    : check IF "invalid return stack address" panic END ;
    dup sys:info:rs <    check
    dup sys:info:size >= check ;
;


( ===== Memory 2 ===== )

: here  ( -- & )
  : addr   0x08 ;
  : align! addr @ align valid:dict addr ! ;
  addr @ ;
: here! ( v -- ) valid:dict here:addr ! ;

: ,     ( v -- ) here  ! here 1 cells + here! ;
: b,    ( b -- ) here b! here 1       + here! ;

: allot ( bytes -- addr )
  here >r  here + align here! r> ;


( ===== Stack 2 ===== )

: pick ( n -- v ) 2 + cells sp + valid:ds @ ;
  # example:
  #   1 2 3 0 pick => 1 2 3 3
  #   1 2 3 2 pick => 1 2 3 1
  # stack:
  #   sp: |
  #       | n
  #       | ...
  # target address: sp + (n+2)*cells


: rpick ( n -- v ) 2 + cells rp + valid:rs @ ;
  # rstack:
  #   rp: |
  #       | caller
  #   n=0 | ...
  #   n=1 | ...
  # target address: rp + (n+2)*cells


: i ( -- v ) 2 cells rp + valid:rs @ ;
: j ( -- v ) 3 cells rp + valid:rs @ ;



( ===== Return stack ===== )

: IFRET ( ret if tos is true ) IF rdrop END ;

: ;IF ( ? q -- ... )
  ( call q and exit from caller if ? is true )
  swap IF rdrop >r RET END drop ;

: ;CASE ( a b q -- ... | a )
  # if a=b call q and escape from caller
  # or ramain a
  >r over = IF drop r> rdrop >r RET END rdrop ;

: ;EQ ( a b -- yes | a )
  # same as [ yes ] ;CASE
  over = IF drop rdrop yes END ;

: ;INIT ( v q -- v | inited )
  # v != 0: return v
  # v  = 0: call q and dup its value
  over IF drop rdrop RET END
  swap drop call dup ;

: init! ( addr q -- v | inited )
  # v of addr != 0: return v
  # else: call q, set v(tos) to addr and return v
  over @ IF 2drop RET END
  swap >r call dup r> ! ;


( ===== String 2 ===== )

: s= ( s1 s2 )
  : same? ( s1 s2 -- c yes | no )
    b@ swap b@ dup >r != IF no RET END r> yes ;
  : loop ( s1 s2 -- ? )
    2dup same? not IF 2drop no RET END ( c )
    0 = IF 2drop yes RET END
    1 + swap 1 + AGAIN ;
  loop ;

: s
  : copy ( src dst -- )
    : write ( dst b -- b ) dup >r swap b! r> ;
    : loop ( dst src -- )
      2dup b@ write 0 = IF 2drop RET END
      1 + swap 1 + swap AGAIN ;
    swap loop
  ;
  : put ( s -- & ) # put to dict
    : loop ( s -- )
      dup b@ dup b, 0 = IF drop RET END 1 + AGAIN ;
    here swap loop here:align! ;
  : len ( s -- n )
    : loop ( n s ) dup b@ IF 1 + swap 1 + swap AGAIN END drop ;
    0 swap loop ;
;


( ===== Random by software ===== )

: xorshift
  val: s
  : gen ( -- v )
    s  13 bit-lshift s bit-xor s!
    s -17 bit-lshift s bit-xor s!
    s   5 bit-lshift s bit-xor dup s!
  ;
  s 0 = IF 2463534242 s! END
  gen
;



( ===== Random by I/O ===== )

: rand ( n -- r )
  # 0 <= r < n
  : query 2 io ;
  : seed! 1 query ;
  : init  2 query ;
  0 query
;



( ===== File ===== )

: file
  val: path
  : query 8 io ;
  : open    0 query ; # path mode -- id ok | ng
  : close   1 query ; # id -- ?
  : read    2 query ; # buf len id -- ?
  : write   3 query ; # buf len id -- ?
  : seek    4 query ; # offset origin id -- ?
  : exists? 5 query ; # path -- ?
  ( --- defensive --- )
  : open!  over path! open IF RET END "Can't open " epr path eprn die ;
  : close! close drop ;
  : read!  read  IF RET END "Can't read" panic ;
  : write! write IF RET END "Can't write" panic ;
;