: immediate read \ exit br ?br + - * % / lshift rshift 
and or invert xor 1+ 1- = < > @reg @ pick @str 
!reg ! !var !str key emit dup drop swap over >r r> 
tail ' , printnum get_word strlen isnumber strnequ find 
execute kernel

\ Howe Forth: Start up code.
\ @author         Richard James Howe.
\ @copyright      Copyright 2013 Richard James Howe.
\ @license        LGPL      
\ @email          howe.r.j.89@gmail.com
\ This notice cannot go at the top of the file, comments
\  will not work until the comment symbol is read in.

: true 1 exit
: false 0 exit

: CPF 13 exit \ Compile Flag 
: state CPF !reg exit
: ; immediate 
	' exit , 
	false state
exit

: r 3 ;	\ Return stack pointer
: v 4 ;	\ Variable stack pointer
: h 5 ;	\ Dictionary pointer
: str 6 ; \ String storage pointer
: here h @reg ;
: pwd 7 ;
: iobl 21 ; \ I/O buf len address

\ Error handling!
: EXF 14 ;
: on_err read on_err ;
find on_err EXF !reg

\ change to command mode
: [ immediate false state ;
\ change to compile mode
: ] true state ;

: _push 0 ;
: _compile 1 ;
: _run 2 ;
: _define 3 ;
: _immediate 4 ;
: _read 5 ;
: _comment 6 ;
: _exit 7 ;

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

\ : ?dup dup if dup then ;
\ : negate -1 * ;
\ : abs dup 0 < if negate then ;
\ : -rot rot rot ;
\ : incrementer create , does> dup dup @ 1+ swap ! @ ;

: 2drop drop drop ;
: 2dup over over ;

: 2+ 1+ 1+ ;
: 2- 1- 1- ;

: swap- swap - ;

: if immediate 
	' ?br , 
	here 0 , 
;

: else immediate
	' br ,
	here
	0 ,
	swap dup here swap-
	swap !
;
: then immediate 
	dup here 
	swap- swap 
	! 
;

: here- here - ;

: begin immediate
	here
;
: until immediate
	' ?br ,
	here- ,
;

: allot here + h !reg ;
: :noname immediate here _run , ] ;
: ? 0= if \ ( bool -- ) ( Conditional evaluation )
      [ find \ literal ] execute 
    then 
;

: _( \ ( key to halt on -- )
    >r
     begin 
        key dup r> dup >r  = dup 
        if 
            nip
        else 
            swap emit 
        then 
    until 
    r> drop
;

: .( 
    ')' _(
;

\ Print out a string stored in string storage
: prn \ ( str_ptr -- )
    begin
        dup @str dup 0= \ if null
        if
            2drop 1       
        else
            emit 1+ 0
        then
    until
;

\ Store a '"' terminated string in string storage
: _." \ ( -- )
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
    CPF @reg 0= if
    '"' _(
    else
        _push , str @reg ,
        _."
        ' prn ,
    then
;
 
: :: 	\ compiles a ':'
  [ find : , ]
;

\ Helper words for create
: '', ' ' , ;       \ A word that writes ' into the dictionary
: ',, ' , , ;       \ A word that writes , into the dictionary
: 'exit, ' exit , ; \ A word that write exit into the dictionary
: 3+ 2+ 1+ ;

\ The word create involves multiple levels of indirection.
\ It makes a word which has to write in values into
\ another word
: create immediate              \ This is a complicated word! It makes a
                                    \ word that makes a word.
  CPF @reg if                   \ Compile time behavour
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
  'exit,                      \ Write in an exit, we don't want the
                                  \ defining to run it, but the *defined* word to.
  here swap !                \ Patch in the code fields to point to.
  _run ,                        \ Write a run in.
;

: constant create , does> @ ; 
: variable create , does> ;
: array create allot does> + ;

: ban \ ( x -- )
	begin 
		[ key = literal ] 
		emit 1- dup 0= 
	until 
	cr drop 
;

: 40ban 40 ban ;

: _show
    2dup - @ prnn tab 1- dup 0=
;
: show \ ( from to -- )
    40ban
    ." Dictionary contents: " cr
    40ban
    tuck swap-
    begin
        2dup - prnn ." :" tab 
        _show if 2drop cr 40ban exit then 
        _show if 2drop cr 40ban exit then 
        _show if 2drop cr 40ban exit then 
        _show
        cr
    until
    2drop
    40ban
;

: _shstr
    2dup - @str emit tab 1- dup 0=
;
: shstr \ ( from to -- )
    40ban
    ." String Storage contents: " cr
    40ban
    tuck
    swap-
    begin
        2dup - prnn ." :" tab 
        _shstr if 2drop cr 40ban exit then 
        _shstr if 2drop cr 40ban exit then 
        _shstr if 2drop cr 40ban exit then 
        _shstr
        cr
    until
    2drop
    40ban
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
        @ dup @ 0=   
    until
    cr
    2drop
;

\ Store filenames (temporary) here.
str @reg dup iobl @reg + str !reg constant filename

\ file i/o
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
  v @reg 1-
  begin
    dup pick prnn space
    1- dup 0= 
  until
  drop
  cr
;

\ Shows the header of a word.
: header
  find 2- dup 40 + show
;



\ ANSI terminal color codes
 'esc' emit .( [2J) cr       \ Reset
 'esc' emit .( [0;0H ) cr
 'esc' emit .( [32;1m) cr    \ Green fg
 .( Howe Forth ) cr .( Base System Loaded ) cr
 .( @author         Richard James Howe. ) cr
 .( @copyright      Copyright 2013 Richard James Howe. ) cr
 .( @license        LGPL ) cr
 .( @email          howe.r.j.89@gmail.com ) cr
 .( Memory Used: ) cr 
 .(   Dictionary: ) here 4 * str @reg + .
 .(   Strings:    ) str @reg . cr
 'esc' emit .( [31;1m) .( OK ) cr 'esc' emit .( [30;1m) cr
