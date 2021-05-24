# Reference


## Principles (current)

- [Save slow code](https://bn.dodgson.org/bn/2005/08/21/)
  - Defensive
    - Find bugs as early as possible
  - Readable


## Glossaries

- TOS: Top of stack
- DS: Data stack
- RS: Return stack



## Naming Convention

- Words for control flow use UPPERCASE
- word_name
- variable_name
- CONSTANT_NAME



## Arkam VM

- Simple Stack Machine
- Little Endian
- 1 Cell = 32bit
- Can access byte-by-byte
- DS size: 512 cells
- RS size: 512 cells
- 2MiB Flat Memory (Code + DS + RS + Heap)
- Instruction and Word
  - LSB of Instruction should be set to 1 (odd)
  - Word address should be aligned to 4bytes(1 cell)
- Provide `step single instruction`
- SP(data stack pointer) and RP(return stack pointer) can be set via Instruction
  - For bound checking. Not memory mapped
- Attachable I/O devices
  - SYS is only a provided device by default
- Halt instruction just returns ARK_HALT code. No quitting a process
- Heap area will be managed by Sol or other compilers


## SarkamVM (SDL Arkam)

- Video
  - Switching FG/BG buffers
    - Can be written via I/O call
  - RGB888


## Future

- Variable Stack Size
- Variable Memory Size


## I/O

`io ( ... op device -- ... )`

op -1 is query for ready.


### 0 SYS

```
0 memory_size ( -- bytes )

2 data_stack_size ( -- cells )

3 data_stack_addr ( -- addr )

4 return_stack_size ( -- cells )

5 return_stack_addr ( -- addr )

6 cell_size ( -- bytes )
  
7 maximum_integer ( -- n )

8 minimum_integer ( -- n )
```


### 1 STDIO

port: 1=stdout 2=stderr

```
0 putc ( c -- )

1 getc ( -- c )

2 port? ( -- n )

3 port! ( n -- )
```


### 2 RANDOM

xorshift.

```
0 gen ( n -- r )
  0 <= r < n

1 seed! ( n -- )

2 init! ( -- )
  init seed randomly ( depends on implemantation )
```


### 3 VIDEO(PPU)

Palette Color: RGB888 in SARKAM

Color number: 0-3

Palette number: 0-63

Pixel: (PaletteNumber * 4) + ColorNumber

fg/bg buffer is bytes of pixels.

Sprite number: 0-255

```
0 set_palette_color! ( color i -- )
  i: color number

1 set_current_color_number! ( i -- )

2 set_current_palette_number! ( i -- )

3 get_current_palette_number ( -- i )

4-9 reserved


10 clear
  bg buffer with current color

11 plot ( x y -- )
  to bg buffer with current color

12 ploti ( i -- )
  to bg buffer with current color

13 switch
  bg buffer to fg

14 transfer ( addr -- )
  to bg buffer

15 copy ( -- )
  copy fg to bg

16 width ( -- w )
  pixels

17 height ( -- h )
  pixels

18,19: reserved


20 set_current_sprite_number ( i -- )

21 load_sprite ( addr -- )
  Current sprite refers addr

22 plot_current_sprite ( x y -- )
  to bg
```


### 4 AUDIO (FM Synth)

Registers

- current voice(ch)
- current operator

```
0 set_current_voice ( i -- )

1 set_current_operator ( i -- )

2 play ( freq -- )
  play current voice

3 stop ( -- )
  stop current voice

4 set_param ( v param -- )
  set parameter of current operator of current voice
  v: 0-255
  param:
    0 atack time
    1 decay time
    2 sustain level (relative to operator volume)
    3 release time
    4 modulation ratio
    5 feedback ratio
    6 wave table
      0 sine
      1 saw
      2 square
      3 noise (reset each note-on)

5 set_algo ( algo -- )
  algo: 0-7
  unstable
```


### 5 KEY

Unimplemented


### 6 MOUSE

```
0 addr_pos ( &x &t -- )
  addr to be set current position

1 addr_left ( &x &y &press -- )
  addr to be set current left click status

2 addr_right ( &x &y &press -- )
  addr to be set current right click status
```


### 7 PAD

Unimplemented


### 8 FILE

```
0 open ( &fname &mode -- id ok | ng )

1 close ( id -- ? )

2 read ( &buf size id -- ? )

3 write ( &buf size id -- ? )

4 seek ( offset origin id -- ? )
  origin: 0 SEEK_SET, 1 SEEK_CUR, 2 SEEK_END

5 access ( path -- ? )
```


### 9 DATETIME

Unimplemented


### 10 SOCKET

Unimplemented


### 11 EMU

Emulator Operation.

```
0 set_title ( s -- )
  window title

1 cursor_show ( ? -- )
   0: hide
  -1: show

2 poll_steps ( n -- )
  steps to poll device events

3 poll ( -- )
  force polling
```


### 12 APP

For handling Application Process such as command line arguments.

```
0 argc ( -- n )

1 read_argc ( addr i len -- ok | ng )
  read arg(i) to addr
  ng: overflow
```



## Sol - Forth like assembly

- Forth without Immediate mode
  - For simplicity of VM/Language separation


### Design Dicisions

- C String
- Use tail recursion instead of loop
- Rename `exit` to `RET`


### Future

- Immediate mode, turnkey, and tree-shaker
- Self hosting


## Entrypoint

Word named `main` should be defined.

Its returning integer or default pushed `0` will be used as system exit code.

So you can write like below.

```
: main ( -- code ) foo bar 42 ; ( returns 42 )
: main ( -- ) foo bar ;         ( returns 0 )
```


### Comment

```
( this is comment ) 
# this is line comment
(this_is_not_a_comment)
```

Left parenthesis requires one or more spaces right after.


### Word definition

```
: inc ( n -- n+1 ) 1 + ;
```

`1 inc` leaves `2` to TOS.


### Quotation

```
: bi-inc ( a b -- a+1 b+1 ) [ 1 + ] bia ;
: foo 2 3 bi-inc + ;
```

`foo` runs `3 4 +` and leaves `7`.


### Word reference

```
: inc ( n -- n+1) 1 + ;
: bi-inc ( a b -- a+1 b+1 ) &inc bia ;
: foo 2 3 bi-inc + ;
```

`foo` runs `3 4 +` and leaves `7`.


### val

```
val: x
```

creates below

```
x  ( -- v )
x! ( v -- )
```

`&x` returns address of variable cell.


### Nested word

```
val: x

: foo
  val: x
  : bar x ;
  x!
;

: main
  42 x!
  43 foo
  foo:bar
;
```

`main` leaves `43`.

`x` in `foo:bar` refers to `foo:x`
