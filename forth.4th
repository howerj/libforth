\ Howe Forth: Start up code.
\ @author         Richard James Howe.
\ @copyright      Copyright 2013 Richard James Howe.
\ @license        LGPL      
\ @email          howe.r.j.89@gmail.com

: true 1 exit
: false 0 exit

: cpf 13 exit \ Compile Flag 
: state cpf !reg exit
: ; immediate 
	' exit , 
	false state
exit

\ Register constants
: r 3 ;	        \ Return stack pointer
: v 4 ;	        \ Variable stack pointer
: h 5 ;	        \ Dictionary pointer
: str 6 ;       \ String storage pointer
: pwd 7 ;       \ previous word
: exf 14 ;      \ Pointer to execution token, executed on error.
: iobl 21 ;     \ I/O buf len address
: here h @reg ;

\ Error handling!
: on_err read on_err ;  \ This word is executed when an error occurs.
find on_err exf !reg    \ Write the executable token for on_err into the dictionary.

\ change to command mode
: [ immediate false state ;
\ change to compile mode
: ] true state ;

\ These words represent words with no name in the dictionary, the 'invisible' words.
: _push 0 ;
: _compile 1 ;
: _run 2 ;

\ System calls
: reset 0 ;
: fopen 1 ;
: fclose 2 ;
: fflush 3 ;
: remove 4 ;
: rename 5 ;
: rewind 6 ;

\ Constants for system call arguments
: input 0 ;
: output 1 ;

: literal immediate _push , , ;

\ ASCII chars

: 'esc' 27 ;
: '"' 34 ;
: ')' 41 ;

: 0= 0 = ;

: space 32 emit ;
: cr 10 emit ;
: tab 9 emit ;

: prnn 10 swap printnum ;
: . prnn cr ;
: # dup . ;

: tuck swap over ;
: nip swap drop ;
: rot >r swap r> swap ;
: <> = 0= ;
: negate -1 * ;
: -rot rot rot ;

: 2drop drop drop ;
: 2dup over over ;

: 2+ 1+ 1+ ;
: 2- 1- 1- ;

: if immediate 
	' ?br , 
	here 0 , 
;

: else immediate
	' br ,
	here
	0 ,
	swap dup here swap -
	swap !
;

: then immediate 
	dup here 
	swap - swap 
	! 
;

: begin immediate
	here
;
: until immediate
	' ?br ,
	here - ,
;

: ?dup dup if dup then ;
: abs dup 0 < if negate then ;

: allot here + h !reg ;
: :noname immediate here _run , ] ;
: ? 0= if \ ( bool -- ) ( Conditional evaluation )
      [ find \ literal ] execute 
    then 
;

: _( \ ( letter bool -- ) \ 
  >r \ Store bool on return stack for simplicity
     begin 
        key 2dup = \ key in a letter, test if it is equal to out terminator
        if
          2drop 1 \ Drop items, quit loop
        else 
          r> dup >r \ test for bool
          if        \ bool == 1, emit letter, bool == 0 drop it.
             emit 
          else 
             drop 
          then 
          0 \ Continue
        then 
    until 
  r> drop \ Return stack back to normal now.
;

: ( immediate ')' 0 _( ;  ( Now we have proper comments )
: .( immediate ')' 1 _( ; ( Print out word )

 ( Print out a string stored in string storage )
: prn ( str_ptr -- )
    begin
        dup @str dup 0= ( if null )
        if
            2drop 1       
        else
            emit 1+ 0
        then
    until
;




 ( Store a '"' terminated string in string storage )
: _." ( -- )
    str @reg 1-
    begin
        key dup >r '"' =
        if
            r> drop 1
        else
            1+ dup r> swap !str 0
        then
    until
    2+
    str !reg
;

: ." immediate
    cpf @reg 0= if
    '"' 1 _(
    else
        _push , str @reg ,
        _."
        ' prn ,
    then
;
 
: :: 	( compiles a ':' )
  [ find : , ]
;

( Helper words for create )
: '', ' ' , ;       \ A word that writes ' into the dictionary
: ',, ' , , ;       \ A word that writes , into the dictionary
: 'exit, ' exit , ; \ A word that write exit into the dictionary
: 3+ 2+ 1+ ;

( The word create involves multiple levels of indirection.
 It makes a word which has to write in values into
 another word )
: create immediate              \ This is a complicated word! It makes a
                                \ word that makes a word.
  cpf @reg if                   \ Compile time behavour
  ' :: ,                        \ Make the defining word compile a header
  '', _push , ',,               \ Write in push to the creating word
  ' here , ' 3+ , ',,           \ Write in the number we want the created word to push
  '', here 0 , ',,              \ Write in a place holder (0) and push a 
                                \ pointer to to be used by does>
  '', 'exit, ',,                \ Write in an exit in the word we're compiling.
  ' false , ' state ,           \ Make sure to change the state back to command mode
  else                          \ Run time behavour
    ::                          \ Compile a word
    _push ,                     \ Write push into new word
    here 2+ ,                   \ Push a pointer to data field
    'exit,                      \ Write in an exit to new word (data field is after exit)
    false state                 \ Return to command mode.
  then
;

: does> immediate
  'exit,                        \ Write in an exit, we don't want the
                                \ defining to run it, but the *defined* word to.
  here swap !                   \ Patch in the code fields to point to.
  _run ,                        \ Write a run in.
;

: constant create , does> @ ; 
: variable create , does> ;
: array create allot does> + ;

( Store temporary filenames temporary here. )
str @reg dup iobl @reg + str !reg constant filename

( file i/o )
: foutput
    filename get_word
    filename output fopen kernel 
;

: finput
    filename get_word
    filename input fopen kernel 
;

: fremove
    filename get_word
    filename remove kernel
;

: fcopy
    foutput
    finput
    begin
        key emit 0
    until
;

: frewind
    output rewind kernel
;

: .s
  v @reg 1- dup 0= if exit then
  begin
    dup pick prnn space
    1- dup 0= 
  until
  drop
  cr
;

0 variable i
0 variable j

: i! i ! ;
: j! j ! ;
: i@ i @ ;
: j@ j @ ;

: do immediate
  ' j! ,
  ' i! ,
  here
;

: not
  if 0 else 1 then
;

: >=
  < not
;

: (++i)>=j
  i@ 1+ i! i@ j@ >= 
;

: loop immediate
  ' (++i)>=j ,
  ' ?br ,
  here - ,
;


: _show i@ @ prnn tab i@ 1+ i!  ;
: show  ( from to -- ) ( Show dictionary storage )
  do
        i@ prnn ." :" tab
        _show _show _show _show i@ 1- i!
        cr
  loop
;

: _shstr
    2dup - @str emit tab 1- 
;
: shstr  ( from to -- ) ( Show string storage contents )
    tuck
    swap -
    begin
        _shstr 
        _shstr 
        _shstr 
        _shstr dup 0 <
        cr
    until
    2drop
;

: regs \ ( -- ) \ Print off register contents
    16 @reg 1- 0 \ register 16 holds the maximum number of registers
    begin
        dup prnn ." :" tab dup @reg . 1+
        2dup =
    until
;

: words
    pwd @reg 
    begin
        dup 1+ @ prn
        space
        @ dup @ 0 =   
    until
    cr
    drop
;

: gcd ( a b -- n )
  begin
    dup
    if
      tuck mod 0
    else
      1
    then
  until
  drop
;

: simplify ( a b -- a/gcd{a,b} b/gcd{a/b} )
  2dup
  gcd
  tuck
  /
  -rot
  /
  swap \ ? check this
;

: 2swap rot >r rot r> ;

: crossmultiply ( a b c d -- a*d b*d c*b d*b )
  rot   ( a c d b )
  2dup  ( a c d b d b )
  *     ( a c d b d*b )
  >r    ( a c d b , d*b )
  rot   ( a d b c , d*b )
  *     ( a d b*c , d*b )
  -rot  ( b*c a d , d*b )
  *     ( b*c a*d , d*b )
  r>    ( b*c a*d d*b )
  tuck  ( b*c d*b a*d d*b )
  2swap ( done! )
;

: realmul ( a/b c/d -- {a/b}*{c/d} )
  rot
  *
  -rot
  *
  swap
  simplify
;

: realdiv ( a/b c/d -- {a/b}/{c/d} )
  swap
  realmul
;

: realadd ( a/b c/d -- {a/b}+{c/d} )
  crossmultiply
  rot
  drop ( or check if equal, if not there is an error )
  -rot
  +
  swap
  simplify
;

: realsub ( a/b c/d -- {a/b}-{c/d} )
  crossmultiply
  rot
  drop ( or check if equal, if not there is an error )
  -rot
  -
  swap
  simplify
;

( Shows the header of a word. )
: header
  find 2- dup 40 + show
;

\ ANSI terminal color codes
: esc 'esc' emit ;
: rst esc ." [2J" cr ;    \ Clear screen
: clr esc ." [0;0H" cr ;  \ Set cursor to 0,0
: red esc ." [31;1m" ;    \ Change text color to red.
: grn esc ." [32;1m" ;    \ ...to green.
: blu esc ." [34;1m" ;    \ ...to blue.
: nrm esc ." [0m" cr ;    \ ...to the default.

: cursor esc ." [" prnn ." ;" prnn ." H" fflush kernel ;

\ : vocabulary create pwd @reg , does> @ pwd !reg ;
\ vocabulary forth

\ Welcome message.
rst clr grn
.( Howe Forth ) cr .( Base System Loaded ) cr
.( @author         ) blu .( Richard James Howe. ) grn cr
.( @copyright      ) blu .( Copyright 2013 Richard James Howe. ) grn cr
.( @license        ) blu .( LGPL ) grn cr
.( @email          ) blu .( howe.r.j.89@gmail.com ) grn cr
.( Memory Used: ) cr 
.(   Dictionary:   ) blu here 4 * str @reg + . grn
.(   Strings:      ) blu str @reg . grn
red .( OK ) nrm
