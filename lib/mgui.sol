include: "sarkam.sol"
include: "entity.sol"


: mouse
  ( positions )
  val: x  val: y
  val: lx val: ly val: lp
  val: rx val: ry val: rp

  : init
    &x &y       mouse:pos!
    &lx &ly &lp mouse:left!
    &rx &ry &rp mouse:right!
  ;
  : listen emu:poll ;
;



: sprite_button
  const: max 128
  val: buttons
  val: sps ( sprites )
  val: params ( to pass to callback )
  val: ss ( show? )  val: qs ( callback )
  val: ps ( pressed? )
  val: xs  val: ys
  val: dx  val: dy  ( draw origin )
  val: i  val: mx  val: my  val: mp ( mouse )
  : sp    i sps    ecs:get ;  : sp!    i sps    ecs:set ;
  : param i params ecs:get ;  : param! i params ecs:set ;
  : x     i xs     ecs:get ;  : x!     i xs     ecs:set ;
  : y     i ys     ecs:get ;  : y!     i ys     ecs:set ;
  : q     i qs     ecs:get ;  : q!     i qs     ecs:set ;
  : p     i ps     ecs:get ;  : p!     i ps     ecs:set ;
  : show  i ss     ecs:get ;  : show!  i ss     ecs:set ;
  : new buttons ecs:components ;
  : init
    max ecs:entities buttons!
    new sps!  new params!  new ss!  new qs!  new ps! new xs!  new ys! ;
  : create ( param x y sp q -- )
    buttons ecs:new IF "too many sprite buttons!" panic END i!
    q! sp! y! x! param! yes show! ;
  : delete ( i -- ) i buttons ecs:delete ;
  : clicked? mp IF no RET END p ;
  : hover? mx my x y 8 8 hover_rect? ;
  : handle_click
    hover? not IF no p! RET END
    clicked? IF no p! param q call END
    mp IF
      yes p!
      dy 1 + dy!
    ELSE
      dy 1 - dy!
    END
  ;
  : draw
    x dx! y dy!
    handle_click
    sp ppu:sprite:i!  dx dy ppu:sprite:plot ;
  : draw_all
    mouse:x  mx!
    mouse:y  my!
    mouse:lp mp!
    buttons [ i!
      show IF draw END
    ] ecs:each
  ;
;


: put_text ( x y s -- )
  val: x   val: y
  val: ox  val: s
  const: w 7
  : right x w + x! ;
  : next s 1 + s! ;
  : newline ox x!  y 9 + y! ;
  : draw ( i -- ) ppu:sprite:i! x y ppu:sprite:plot ;
  : loop
    s b@
    dup 0  = IF drop RET END
    dup 10 = IF drop newline next AGAIN END
    dup 32 = IF drop right next AGAIN END
    draw right next AGAIN ;
  s! y! dup x! ox!
  loop
;


: put_hex ( n x y -- )
  val: buf  val: n  val: i  val: q  val: r  val: x  val: y
  const: max  8  ( u32 = 4bytes = FF FF FF FF )
  const: base 16
  : init buf IF RET END max 1 + allot buf! ;
  : check buf i > IF "too big hex" panic END ;
  : >char ( n -- ) dup 10 < IF 48 ELSE 55 END + ;
  : put ( n -- ) >char i b! i 1 - i! ;
  : fin 0 buf i + b! ;
  : read check
    n base /mod r! q!
    r put
    q 0 = IF RET END q n! AGAIN ;
  y! x! n!  init  buf max + i! read x y i 1 + put_text
;


: put_ff ( n x y -- )
  const: base 16
  val: buf  val: x  val: y
  : init buf IF RET END 3 allot buf! ;
  : >char ( n -- ) dup 10 < IF 48 ELSE 55 END + ;
  init
  y! x! 0xff bit-and base /mod ( q r )
  >char buf 1 + b!
  >char buf     b!
  x y buf put_text
;


: text_button
  const: max 128
  val: buttons
  val: ts ( texts )
  val: params ( to pass to callback )
  val: ss ( show? )  val: qs ( callback )
  val: ws ( width )
  val: ps ( pressed? )
  val: xs  val: ys
  val: dx  val: dy  ( draw origin )
  val: i  val: mx  val: my  val: mp ( mouse )
  : t      i ts     ecs:get ;  : t!     i ts     ecs:set ;
  : param  i params ecs:get ;  : param! i params ecs:set ;
  : p      i ps     ecs:get ;  : p!     i ps     ecs:set ;
  : x      i xs     ecs:get ;  : x!     i xs     ecs:set ;
  : y      i ys     ecs:get ;  : y!     i ys     ecs:set ;
  : q      i qs     ecs:get ;  : q!     i qs     ecs:set ;
  : w      i ws     ecs:get ;  : w!     i ws     ecs:set ;
  : show   i ss     ecs:get ;  : show!  i ss     ecs:set ;
  : new buttons ecs:components ;
  : init
    max ecs:entities buttons!
    new ts! new params! new ws! new ss!  new qs!  new ps! new xs!  new ys! ;
  : create ( param x y text q -- i )
    buttons ecs:new IF "too many text buttons!" panic END i!
    q! t! y! x! param!
    t s:len put_text:w * w!
    yes show! ;
  : delete ( i -- ) buttons ecs:delete ;
  : clicked? mp IF no RET END p ;
  : hover? mx my x y w 8 hover_rect? ;
  : handle_click
    hover? not IF no p! RET END
    clicked? IF no p! param q call END
    mp IF
      yes p!
      dy 1 + dy!
    ELSE
      dy 1 - dy!
    END
  ;
  : draw
    x dx! y dy!
    handle_click
    dx dy t put_text
    3 ppu:color!
    dx  dy 9 +  dx w + 1 -  dy 9 + line
    ;
  : draw_all
    mouse:x  mx!
    mouse:y  my!
    mouse:lp mp!
    buttons [ i!
      show IF draw END
    ] ecs:each
  ;
;



: mgui
  : init
    mouse:init
    sprite_button:init
    text_button:init
  ;
  : update
    mouse:listen
    sprite_button:draw_all
    text_button:draw_all
  ;
;
