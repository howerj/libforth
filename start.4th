#!./forth

( @todo Different machine word sizes need testing, which may affect
  functions which access memory by character )

( Welcome to libforth, A dialect of Forth. Like all versions of Forth this 
version is  a little idiosyncratic, but how the interpreter works is
documented here and in various other files.

This file contains most of the start up code, some basic start up code
is executed in the C file as well which makes programming at least bearable.
Most of Forth is programmed in itself, which may seem odd if your back
ground in programming comes from more traditional language [such as C], 
although decidedly less so if you know already know lisp, for example.

The main contributions that this file brings on top of the normal
interpreter are "create", "does>", "do", "loop", "hide", "words",
"variable", "array" and "constant".

For more information about this interpreter and Forth see: 
	https://en.wikipedia.org/wiki/Forth_%28programming_language%29
	libforth.md : for more information about this interpreter
	libforth.h  : for information about the C API
	libforth.3  : for limited information about the C API
	libforth.c  : for the interpreter itself

The interpreter and this code originally descend from a Forth interpreter
written in 1992 for the International obfuscated C Coding Competition 

See:
	http://www.ioccc.org/1992/buzzard.2.design
)

: logical ( x -- bool ) not not ;
: hidden-mask 0x80 ( Mask for the hide bit in a words MISC field ) ;
: instruction-mask 0x1f ( Mask for the first code word in a words MISC field ) ;
: hidden? hidden-mask and logical ;
: dictionary-start 
	( The dictionary start at this location, anything before this value 
	is not a defined word ) 
	64 
;
: source ( -- c-addr u ) 
	32 size * ( size of input buffer, in characters )
	64 size * ( start of input buffer, in characters )
;

: again immediate 
	( loop unconditionally in a begin-loop:
		begin ... again )
	' jmp , here - , 
; 
: 2drop ( x y -- ) drop drop ;
: words ( -- )
	( This function prints out all of the defined words, excluding hidden words.
	An understanding of the layout of a Forth word helps here. The dictionary
	contains a linked list of words, each forth word has a pointer to the previous
	word until the first word. The layout of a Forth word looks like this:

	NAME:  Forth Word - A variable length ASCII NUL terminated string
	PWD:   Previous Word Pointer, points to the previous word
	MISC:  Flags, code word and offset from previous word pointer to start of Forth word string
	CODE/DATA: The body of the forth word definition, not interested in this.
	
	There is a register which stores the latest defined word which can be
	accessed with the code "pwd @". In order to print out a word we need to
	access a words MISC field, the offset to the NAME is stored here in bits
	8 to 15 and the offset is calculated from the PWD field.

	"print" expects a character address, so we need to multiply any calculated
	address by the word size in bytes. )
	pwd @ ( Load most recently defined word )
	begin 
		dup dup     ( Make some copies: pwd pwd pwd )
		1+ @ dup    ( Load Flags fields: misc misc pwd pwd )
		hidden? not ( If the word is hidden, do not print it: hidden? pwd-1 pwd pwd ) 
		if 
			8 rshift 255 and ( offset pwd pwd )
			- size *  ( word-name-address pwd )
			print tab  ( pwd )
		else 
			2drop ( pwd )
		then 
		@  ( Get pointer to previous word )
		dup dictionary-start < ( stop if pwd no longer points to a word )
	until 
	drop cr 
; 

: false 0 ;
: true 1 ;
: 2- ( x -- x ) 2 - ;
: 2+ ( x -- x ) 2 + ;
: 3+ ( x -- x ) 3 + ;
: +! ( x addr -- ) ( add x to a value stored at addr ) tuck @ + swap ! ;
: mod ( x u -- remainder ) 2dup / * - ;
: char key drop key ;
: [char] immediate char ;

: hide ( WORD -- hide-token ) 
	( This hides a word from being found by the interpreter )
	find 1+ dup 
	if 
		dup @ hidden-mask or swap tuck ! 
	else 
		drop 0 
	then 
;

: unhide ( hide-token -- ) dup @ hidden-mask invert and swap ! ;
: push-location 2 ;       ( location of special "push" word )
: compile-instruction 1 ; ( compile code word, threaded code interpreter instruction )
: run-instruction 2 ;     ( run code word, threaded code interpreter instruction )
: literal immediate push-location , , ; 
: ?dup ( x -- ? ) dup if dup then ;
: min ( x y -- min ) 2dup < if drop else swap drop then  ; 
: max ( x y -- max ) 2dup > if drop else swap drop then  ; 
: >= ( x y -- bool ) < not ;
: <= ( x y -- bool ) > not ;
: 2* ( x -- x ) 1 lshift ;
: 2/ ( x -- x ) 1 rshift ;
: #  ( x -- x ) dup . cr ;
: 2! swap over ! 1+ ! ;
: 2@ dup 1+ @ swap @ ;
: r@ r> r @ swap >r ;
: 0> 0 > ;
: 0<> 0 <> ;

: usleep clock +  begin dup clock < until drop ;
: sleep 1000 * usleep ;

: gcd ( a b -- n ) ( greatest common divisor )
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

: log-2 ( x -- log2 ) 
	( Computes the binary integer logarithm of a number,
	zero however returns itself instead of reporting an error )
	0 swap 
	begin 
		swap 1+ swap 2/ dup 0= 
	until 
	drop 1- 
;

( The following section defines a pair of words "create" and "does>" which 
are a powerful set of words that can be used to make words that can create
other words. "create" has both run time and compile time behavior, whilst
"does>" only works at compile time in conjunction with "create". These two
words can be used to add constants, variables and arrays to the language,
amongst other things.

A simple version of create is as follows  
	: create :: 2 , here 2 + , ' exit , 0 state ! ; 
But this version is much more limited )

: write-quote ' ' , ;   ( A word that writes ' into the dictionary )
: write-comma ' , , ;   ( A word that writes , into the dictionary )
: write-exit ' exit , ; ( A word that write exit into the dictionary ) 

: state! ( bool -- ) ( set the compilation state variable ) state ! ;

: _create             \ create a new work that pushes its data field
	::            \ compile a word
	push-location ,       \ write push into new word
	here 2+ ,     \ push a pointer to data field
	' exit ,      \ write in an exit to new word data field is after exit 
	false state!  \ return to command mode
;

: create immediate              \ This is a complicated word! It makes a
                                \ word that makes a word.
  state @ if                    \ Compile time behavior
  ' :: ,                        \ Make the defining word compile a header
  write-quote push-location , write-comma    \ Write in push to the creating word
  ' here , ' 3+ , write-comma        \ Write in the number we want the created word to push
  write-quote here 0 , write-comma   \ Write in a place holder 0 and push a 
                                     \ pointer to to be used by does>
  write-quote write-exit write-comma \ Write in an exit in the word we're compiling.
  ' false , ' state! ,          \ Make sure to change the state back to command mode
  else                          \ Run time behavior
        _create
  then
;
hide _create drop
hide state! drop

: does> ( whole-to-patch -- ) 
  immediate
  write-exit                    \ Write in an exit, we don't want the
                                \ defining to run it, but the *defined* word to.
  here swap !                   \ Patch in the code fields to point to.
  run-instruction ,             \ Write a run in.
;

hide write-quote drop
hide write-comma drop

: array    create allot does> + ;
: variable create ,     does>   ;
: constant create ,     does> @ ;


( === End Create === )

: inc dup @ 1 + swap ! exit

( do loop : This section as the do-loop looping constructs )

: do immediate
	' swap ,      ( compile 'swap' to swap the limit and start )
	' >r ,        ( compile to push the limit onto the return stack )
	' >r ,        ( compile to push the start on the return stack )
	here          ( save this address so we can branch back to it )
;

: inci 
	r@ 1-        ( get the pointer to i )
	inc          ( add one to it )
	r@ 1- @      ( find the value again )
	r@ 2- @      ( find the limit value )
	<
	if
		r@ @ @ r@ @ + r@ ! exit ( branch )
	then
	r> 1+
	r> drop
	r> drop
	>r
;
  	
: loop immediate ' inci , here - , ;

hide inci drop

: leave
	( break out of a do-loop construct )
	r> drop ( pop off our return address )
	r> drop ( pop off i )
	r> drop ( pop off the limit of i and return to the caller's caller routine )
;

: i ( -- i ) 
	( Get current, or innermost, loop index in do-loop construct )
	r> r> ( pop off return address and i ) 
	tuck  ( tuck i away ) 
	>r >r ( restore return stack ) 
;

: range   ( nX nY -- nX nX+1 ... nY )  swap 1+ swap do i loop ;
: repeat  ( n0 X -- n0 ... nX )        1 do dup loop ;
: sum     ( n0 ... nX X -- sum<0..X> ) 1 do + loop ;
: mul     ( n0 ... nX X -- mul<0..X> ) 1 do * loop ;

: factorial ( n -- n! )
	( This factorial is only here to test range, mul, do and loop )
	dup 1 <= 
 	if
		drop
		1
	else ( This is obviously super space inefficient )
  		dup >r 1 range r> mul
	then
;

: cell+ 1+ ( a-addr1 -- a-addr2 ) ;
: cells ( n1 -- n2 ) ;
: char+ ( c-addr1 -- c-addr2 ) 1+ ;
: chars ( n1 -- n2 )  size / ;
: decimal ( -- ) ( print out decimal ) 0 hex ;
: negate -1 * ;

: execution-token ( previous-word-address -- execution-token )
        ( Given the address of the PWD field of a word this
	function will return an execution token for the word )
	1+    ( MISC field )
	dup 
	@     ( Contents of MISC field )
	instruction-mask and  ( Mask off the instruction )
        ( If the word is not an immediate word, execution token pointer ) 
        compile-instruction = +
;

: tail 
	( This function implements tail calls, which is just a jump
        to the beginning of the words definition, for example this
        word will never overflow the stack and will print "1" followed
        by a new line forever,
    
		: forever 1 . cr tail ; 

	Whereas

		: forever 1 . cr recurse ;

	or

		: forever 1 . cr forever ;

	Would overflow the return stack. )
	immediate
	pwd @ execution-token 
	' jmp ,
	here - 1+ ,
;

: recurse
	immediate
	( This function implements recursion, although this interpreter
	allows calling a word directly. If used incorrectly this will
	blow up the return stack.

	We can test "recurse" with this factorial function:
	  : factorial  dup 2 < if drop 1 exit then dup 1- recurse * ;)
	pwd @ execution-token ,
;
hide execution-token drop

: bl ( space character ) [char]  literal ; ( warning: white space is significant after [char] )
: space bl emit ;
: spaces ( n -- ) 0 do space loop ;

: 8* ( x -- x ) 8 * ;
: mask-byte ( x -- x ) 8* 0xff swap lshift ;
: alignment-bits [ 1 size log-2 lshift 1- literal ] and ;

: c@ ( character-address -- char )
	( retrieve a character from an address )
	dup chars @
	>r 
		alignment-bits 
		dup 
		mask-byte 
	r> 
	and swap 8* rshift
;

: c! ( char character-address -- )
	( store a character at an address )
	dup 
	>r          ( new addr -- addr )
		alignment-bits 8* lshift ( shift character into correct byte )
	r>          ( new* addr )
	dup chars @ ( new* addr old )
	over        ( new* addr old addr ) 
	alignment-bits mask-byte invert and ( new* addr old* )
	rot or swap chars !
;

0 variable x
: x! ( x -- ) x ! ;
: x@ ( -- x ) x @ ;

: 2>r ( x1 x2 -- R: x1 x2 )
	r> x! ( pop off this words return address )
		swap
		>r    
		>r    
	x@ >r ( restore return address )
;

: 2r> ( R: x1 x2 -- x1 x2 )
	r> x! ( pop off this words return address )
		r>
		r>
		swap
	x@ >r ( restore return address )
;


: 2r@ ( -- x1 x2 ) ( R:  x1 x2 -- x1 x2 )
	r> x! ( pop off this words return address )
	r> r>
	2dup
	>r >r
	swap
	x@ >r ( restore return address )
;

: pick ( xu ... x1 x0 u -- xu ... x1 x0 xu )
	stack-start depth + swap - 1- @
;

: unused ( -- u ) ( push the amount of core left ) max-core here - ;

0 variable delim
: accepter
	( c-addr max delimiter -- i )
	( store a "max" number of chars at c-addr until "delimiter" encountered,
          the number of characters stored is returned )
	key drop ( drop first key after word )
	delim !  ( store delimiter used to stop string storage when encountered)
	0
	do
		key dup delim @ <>
		if
			over c! 1+ 
		else ( too many characters read in )
			2drop
			i
			leave
		then
	loop
	begin ( read until delimiter )
		key delim @ =
	until
;
hide delim drop


255 constant max-string-length 

: '"' [char] " literal ;
: accept swap '\n' accepter ;
: accept-string max-string-length '"' accepter  ;

0 variable delim
: print-string 
	( delimiter -- )
	( print out the next characters in the input stream until a 
	"delimiter" character is reached )
	key drop
	delim !
	begin 
		key dup delim @ = 
		if 
			drop exit 
		then 
		emit 0 
	until 
;
hide delim drop

size 1- constant aligner 
: aligned 
	aligner + aligner invert and
;
hide aligner drop

: align ( x -- x ) ; ( nop in this implementation )

0 variable delim
: write-string ( char -- c-addr u )
	( Write a string into word being currently defined, this
	code has to jump over the string it has just put into the
	dictionary so normal execution of a word can continue. The
	length and character address of the string are left on the
	stack )
	delim !         ( save delimiter )
	' jmp ,         ( write in jump, this will jump past the string )
	here 0 ,        ( make hole )
	dup 1+ size *   ( calculate address to write to )
	max-string-length delim @ accepter dup >r ( write string into dictionary, save index )
	aligned 2dup size / ( stack: length hole char-len hole )
	dup here + h !   ( update dictionary pointer with string length )
	1+ swap !        ( write place to jump to )
	drop             ( do not need string length anymore )
	push-location ,  ( push next value ) 
	1+ size * dup >r ( calculate place to print, save it )
	,                ( write value to push, the character address to print)
	' print ,        ( write out print, which will print our string )
	r> r> swap       ( restore index and address of string )
;
hide delim drop

: do-string ( char -- )
	state @ if write-string 2drop else print-string then
;

: " immediate '"' do-string ;
: .( immediate ')' do-string ;
: ." immediate '"' do-string ;

: ok " ok" cr ;

: count ( c-addr1 -- c-addr2 u ) dup c@ swap 1+ swap ;

: fill ( c-addr u char -- )
	( fill in an area of memory with a character, only if u is greater than zero )
	swap dup
	0> if 
		0
		do 
			2dup i + c!
		loop
	then
	2drop
;

: erase ( addr u )
	size * swap size * 0 -rot fill
;

: blank ( c-addr u )
	( given a character address and length, this fills that range with spaces )
	bl -rot swap fill
;

: move ( addr1 addr2 u -- )
	( copy u words of memory from 'addr2' to 'addr1' if u is greater than zero )
	rot
	dup 
	0> if
		0
		do
			2dup i + @ swap i + !
		loop
	then
	2drop
;

( It would be nice if move and cmove could share more code, as they do exactly
  the same thing but with different load and store functions, cmove>  )
: cmove ( c-addr1 c-addr2 u -- )
	( copy u characters of memory from 'c-addr2' to 'c-addr1' if u is greater than zero )
	rot
	dup 
	0> if
		0
		do
			2dup i + c@ swap i + c!
		loop
	then
	2drop
;

: ) ;

size 2 = ? 0x0123           variable endianess
size 4 = ? 0x01234567       variable endianess
size 8 = ? 0x01234567abcdef variable endianess

: endian ( -- bool )
	( returns the endianess of the processor )
	endianess size * c@ 0x01 =
;
hide endianess

( create a new variable "pad", which can be used as a scratch pad )
0 variable pad 32 allot

( 255 0x8000 accept hello world
drop

8 0x4100 0x4000 move

0x8000 print cr
0x8200 print cr

0x4000 40 erase
0x8000 print cr
0x8200 print cr )


hide write-string drop
hide do-string drop

hide  accept-string        drop
hide  '"'                  drop
hide  ')'                  drop
hide  alignment-bits       drop
hide  print-string         drop
hide  compile-instruction  drop
hide  dictionary-start     drop
hide  hidden?              drop
hide  hidden-mask          drop
hide  instruction-mask     drop
hide  max-core             drop
hide  push-location        drop
hide  run-instruction      drop
hide  stack-start          drop
hide  x                    drop
hide  x!                   drop
hide  x@                   drop
hide  write-exit           drop
hide  jmp                  drop
hide  jmpz                 drop
hide  mask-byte            drop
hide  accepter             drop
hide  max-string-length    drop
hide  line                 drop

( TODO
	* Execute
	* Fix recurse
	* Evaluate [this would require changes to the interpreter]
	* Base
	* Find is not ANS forth compatible
	* By adding "c@" and "c!" to the interpreter I could remove print
	* Word, Parse, ?do, repeat, while, Case statements
	quite easily by implementing "count" in the start up code
)
