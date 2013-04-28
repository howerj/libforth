: immediate read \ exit br ?br + - * % / lshift rshift 
and or invert xor 1+ 1- 0> = < > @reg @dic @var @ret @str 
!reg !dic !var !ret !str key emit dup drop swap over >r r> 
tail ' , printnum get_word strlen isnumber strnequ _find kernel

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
: error 2 ;

: literal immediate _push , , ;

\ Error handling!
: EXF 14 ;
: _on_err read _on_err ;

: on_err ' _on_err EXF !reg ;
on_err

\ ASCII chars

: 'space' 32 ;
: 'esc' 27 ;
: '\n' 10 ;
: '\t' 9 ;
: '"' 34 ;
: ')' 41 ;

: 0= 0 = ;

: cr '\n' emit ;
: tab '\t' emit ;

: prnn 10 swap printnum ;
: . prnn cr ;
: # dup . ;

: tuck swap over ;
: nip swap drop ;
: rot >r swap r> swap ;
: -rot rot rot ;
: <> = 0= ;

: 2drop drop drop ;
: 2dup over over ;

: 2+ 1+ 1+ ;

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
	swap !dic
;
: then immediate 
	dup here 
	swap- swap 
	!dic 
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
        [ '\n' literal ] key <>
        if 
            0 ? 
        then 
    then 
;

: _s \ ( key to halt on -- )
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
    ')' _s
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
    '"' _s
    else
        _push , str @reg ,
        _."
        ' prn ,
    then
;
 
: :: 	\ compiles a ':'
	[ 
		here 1- h !reg	\ back up dictionary pointer
		_define , 	\ write in primitive for ':'
	] 
;

: create 
	:: 
	false state 
;

: constant immediate
	create 
	_push , , 
	' exit , 
; 

: variable immediate
	create
	_push , 
	here 2+ , \ pointer to allocations
	' exit ,	\ over this
	,		\ <-- points here
;

\ There is no bounds checking at the moment...expects NUMBER INDEX
: array immediate
	create
	_push , 
	here 3 + , \ pointer to allocations
	' + ,		\ over this
	' exit ,	\ over this
	allot		\ <-- points here
;

: ban \ ( x -- )
	begin 
		[ key = literal ] 
		emit 1- dup 0= 
	until 
	cr drop 
;

: 40ban 40 ban ;

: @reg. \ ( x -- )
	@reg .
;

: _show
    2dup - @dic prnn tab 1- dup 0=
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

3 constant tabvar
: tabbing \ ( counter X -- counter-1 X )
        swap dup if
            tab 
            1-
        else
            cr
            drop
            tabvar
        then swap
;

: words
    tabvar pwd @reg 
    begin
        dup 1+ @dic prn
        tabbing
        @dic dup 0=   
    until
    cr
    2drop
;

\ Store filenames (temporary) here.
str @reg dup 32 + str !reg constant filename

\ file i/o
: foutput
    filename get_word
    filename output fopen kernel 
;

: finput
    filename get_word
    filename input fopen kernel 
;

: ferror
    filename get_word
    filename error fopen kernel 
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

: [END]
    input fclose kernel 
    output fclose kernel 
    error fclose kernel
    \ drops temporary measure
    2drop drop 
;

: .s
    v @reg tabvar swap
    begin
        dup @var prnn 
        tabbing
        1- dup 0=
    until
    cr
    2drop
;


\ ANSI terminal color codes
'esc' emit .( [2J) cr       \ Reset
'esc' emit .( [32m) cr    \ Green fg
\ 'esc' emit .( [40m) cr    \ Black bg
.( Howe Forth ) cr .( Base System Loaded ) cr
.( Memory Usuage: ) here 2 * str @reg + . cr
'esc' emit .( [31m) .( OK ) cr 'esc' emit .( [30m) cr

[END]
