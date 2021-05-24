include: "sarkam.sol"
include: "entity.sol"
include: "basic_sprite.sol"
include: "mgui.sol"
include: "fm.sol"



: rbeat
  const: max_ch   8
  const: beat_len 8
  const: fpb      6 ( frame per beat )
  const: max_age  10  ( note len )
  ( ----- sequencer ----- )
  val: beat_i
  val: beat_life
  ( ----- channel ----- )
  const: ch_not_playing -1
  val: channels
  val: playing
  val: freqs
  val: lifes
  val: beat_seqs
  : freq  freqs ecs:get ;
  : freq! freqs ecs:set ;
  : life  lifes ecs:get ;
  : life! lifes ecs:set ;
  : beats  ( ch -- a ) beat_seqs ecs:get ;
  : beats! ( a ch -- ) beat_seqs ecs:set ;
  : beat   ( i ch -- b ) beats + b@ ;
  : beat!  ( b i ch -- ) beats + b! ;
  ( ----- play ----- )
  : play val: ch
    ch!
    ch fm:voice!
    ch freq fm:play
    ; # ch --
  : stop fm:voice! fm:stop ; # ch --
  : next_beat beat_i 1 + beat_len mod beat_i! ;
  : play_all
    0 beat_i!
    yes playing!
  ;
  : stop_all
    no playing!
    channels [ stop ] ecs:each
  ;
  ( ----- randomize ----- )
  : rand_ch ( ch -- )
    val: ch
    dup ch! fm:voice!
    8 rand fm:algo!
    440 rand 20 + ch freq!
    ( beats )
    beat_len [ beat_i!
      3 rand IF 0 beat_i ch beat! RET END
      max_age rand 1 + beat_i ch beat!
    ] for
    ( operators )
    4 [ fm:operator!
      10  rand fm:attack!
      10  rand fm:decay!
      30  rand fm:sustain!
      100 rand fm:release!
      18  rand fm:mod_ratio!
      200 rand fm:fb_ratio!
      4   rand fm:wave!
      255 rand fm:ampfreq!
      255 rand fm:fm_level!
    ] for
  ;
  : rand_channels channels [ rand_ch ] ecs:each ;
  ( ----- draw ----- )
  : draw_ch ( ch -- )  val: ch  val: i  val: x  val: y
    ch!
    ch 16 * 8 + y!
    beat_len [ i!
      i 10 * 8 + x!
      i ch beat IF 3 ELSE 1 END ppu:sprite:i!
      x y ppu:sprite:plot
    ] for
  ;
  : draw_pos
    3 ppu:sprite:i!
    beat_i 10 * 8 + 136 ppu:sprite:plot
  ;
  : draw_all
    channels [ draw_ch ] ecs:each
    playing IF draw_pos END
  ;
  ( ----- update ----- )
  : update_ch ( ch -- ) val: ch  val: l
    ch! ch life l!
    l ch_not_playing = IF RET END
    l 0 > IF l 1 - ch life! RET END ( playing )
    ch stop ch_not_playing ch life!
  ;
  : trigger_ch ( ch -- ) val: ch
    ch!
    beat_i ch beat dup IF ch life! ch play RET END
    drop ch stop
  ;
  : shuffle ( frames -- frames )
    beat_i 2 mod IF RET END 2 + ;
  : update
    draw_all
    playing not IF RET END
    beat_life 0 > IF
      beat_life 1 - beat_life!
      channels [ update_ch ] ecs:each
    RET END
    ( trigger beats )
    channels [ dup trigger_ch update_ch ] ecs:each
    fpb shuffle beat_life!
    next_beat
  ;
  ( ----- init ----- )
  : new_components channels ecs:components ;
  : init
    max_ch ecs:entities channels!
    max_ch [ channels ecs:new! drop ] times
    new_components freqs!
    new_components lifes!
    new_components beat_seqs!
    channels [ beat_len allot swap beats! ] ecs:each
    channels [ ch_not_playing swap life! ] ecs:each
    rand_channels
    0 8   148 "play" [ play_all ] text_button:create
    0 58  148 "stop" [ stop_all ] text_button:create
    0 108 148 "random" [ drop rand_channels ] text_button:create
  ;
;



: main_loop
  ppu:0clear
  mgui:update

  rbeat:update

  ppu:switch!
  AGAIN
;


: main
  "rand_fm_beat" emu:title!
  yes emu:show_cursor!
  rand:init
  mgui:init
  basic_sprite:load

  rbeat:init
  rbeat:rand_channels

  main_loop
;
