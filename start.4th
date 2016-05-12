#!./forth
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

( We'll begin by defining very simple words we can use later )
: logical ( x -- bool ) not not ;
: hidden-mask 0x80 ( Mask for the hide bit in a words MISC field ) ;
: instruction-mask 0x1f ( Mask for the first code word in a words MISC field ) ;
: hidden? hidden-mask and logical ;
: push-location 2 ;       ( location of special "push" word )
: compile-instruction 1 ; ( compile code word, threaded code interpreter instruction )
: run-instruction 2 ;     ( run code word, threaded code interpreter instruction )
: literal immediate push-location , , ; 
: false 0 ;
: true 1 ;
: 8* 3 lshift ;
: 2- 2 - ;
: 2+ 2 + ;
: 3+ 3 + ;
: 256/ 8 rshift ;
: mod ( x u -- remainder ) 2dup / * - ;
: */ ( n1 n2 n3 -- n4 ) * / ; ( warning: this does not use a double cell for the multiply )
: char key drop key ;
: [char] immediate char push-location , , ;
: postpone immediate find , ;
: bl ( space character ) [char]  ; ( warning: white space is significant after [char] )
: space bl emit ;
: address-unit-bits size 8* ;
: mask-byte ( x -- x ) 8* 0xff swap lshift ;
: cell+ 1+ ( a-addr1 -- a-addr2 ) ;
: cells ( n1 -- n2 ) ;
: char+ ( c-addr1 -- c-addr2 ) 1+ ;
: chars  ( n1 -- n2: convert character address to cell address ) size / ;
: chars> ( n1 -- n2: convert cell address to character address ) size * ;
: hex     ( -- ) ( print out hex )     16 base ! ;
: octal   ( -- ) ( print out octal )    8 base ! ;
: decimal ( -- ) ( print out decimal )  0 base ! ;
: negate ( x -- x ) -1 * ;
: square ( x -- x ) dup * ;
: +! ( x addr -- ) ( add x to a value stored at addr ) tuck @ + swap ! ;
: 1+! ( addr -- : increment a value at an address )  1 swap +! ;
: 1-! ( addr -- : decrement a value at an address ) -1 swap +! ;
: lsb ( x -- x : mask off the least significant byte of a cell ) 255 and ;
: \ immediate begin key '\n' = until ;
: ?dup ( x -- ? ) dup if dup then ;
: min ( x y -- min ) 2dup < if drop else swap drop then  ; 
: max ( x y -- max ) 2dup > if drop else swap drop then  ; 
: >= ( x y -- bool ) < not ;
: <= ( x y -- bool ) > not ;
: 2* ( x -- x ) 1 lshift ;
: 2/ ( x -- x ) 1 rshift ;
\ : #  ( x -- x ) dup . cr ;
: 2@ dup 1+ @ swap @ ;
: r@ r> r @ swap >r ;
: 0> 0 > ;
: 0<> 0 <> ;
: nand ( x x -- x : Logical NAND ) and not ;
: nor  ( x x -- x : Logical NOR  )  or not ;
: ms ( u -- : wait at least 'u' milliseconds ) clock +  begin dup clock < until drop ;
: sleep 1000 * ms ;
: align immediate ( x -- x ) ; ( nop in this implementation )
: ) immediate ;
: ? ( a-addr -- : view value at address ) @ . cr ;
: bell 7 ( ASCII BEL ) emit ;
: b/buf  ( bytes per buffer ) 1024 ;
: # dup ( x -- x : debug print ). cr ;
: compile, ' , , ;   ( A word that writes , into the dictionary )
: >mark ( -- ) here 0 , ; 
: end immediate (  A synonym for until ) postpone until ;
: bye ( -- : quit the interpreter ) 0 r ! ;

: rdrop ( R: x -- )
	r>           ( get caller's return address )
	r>           ( get value to drop )
	drop         ( drop it like it's hot )
	>r           ( return return address )
;

: ?exit ( x -- ) ( exit current definition if not zero ) if rdrop exit then ;

: dictionary-start 
	( The dictionary start at this location, anything before this value 
	is not a defined word ) 
	64 
;
: source ( -- c-addr u ) 
	[ 32 chars> ] literal ( size of input buffer, in characters )
	[ 64 chars> ] literal ( start of input buffer, in characters )
;

: source-id ( -- 0 | 1 | 2 )
	( The returned values correspond to whether the interpreter is
	reading from the user input device or is evaluating a string,
	currently the "evaluate" word is not accessible from within
	the Forth environment and only via the C-API, however the
	value can still change, the values correspond to:
	Value    Input Source
	2        String
	1        Standard input
	0        File Input [this may be stdin] )
	source-id-reg @
;

: again immediate 
	( loop unconditionally in a begin-loop:
		begin ... again )
	' branch , here - , 
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
		hidden? ( If the word is hidden, do not print it: hidden? pwd-1 pwd pwd ) 
		if 
			2drop ( pwd )
		else 
			256/ lsb ( offset pwd pwd )
			- chars>  ( word-name-address pwd )
			print tab  ( pwd )
		then 
		@  ( Get pointer to previous word )
		dup dictionary-start < ( stop if pwd no longer points to a word )
	until 
	drop cr 
; 

: hide  ( WORD -- hide-token ) 
	( This hides a word from being found by the interpreter )
	find dup 
	if 
		dup @ hidden-mask or swap tuck ! 
	else 
		drop 0 
	then 
;

: hider ( -- ) ( hide with drop ) hide drop ;
: unhide ( hide-token -- ) dup @ hidden-mask invert and swap ! ;
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

: log2 ( x -- log2 ) 
	( Computes the binary integer logarithm of a number,
	zero however returns itself instead of reporting an error )
	0 swap 
	begin 
		swap 1+ swap 2/ dup 0= 
	until 
	drop 1- 
;

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

: ['] immediate find execution-token push-location , , ;

: execute ( execution-token -- )
	( given an execution token, execute the word )

	( create a word that pushes the address of a hole to write to
	a literal takes up two words, '!' takes up one )
	1- ( execution token expects pointer to PWD field, it does not
		care about that field however, and increments past it )
	execution-token
	[ here 3+ literal ] 
	!                   ( write an execution token to a hole )
	[ 0 , ]             ( this is the hole we write )
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

: write-quote ['] ' , ;   ( A word that writes ' into the dictionary )
: write-exit ['] exit , ; ( A word that write exit into the dictionary ) 

: state! ( bool -- ) ( set the compilation state variable ) state ! ;

: command-mode false state! ;

: creater             \ create a new work that pushes its data field
	::            \ compile a word
	push-location ,       \ write push into new word
	here 2+ ,     \ push a pointer to data field
	['] exit ,    \ write in an exit to new word data field is after exit 
	command-mode  \ return to command mode
;


: create immediate              \ This is a complicated word! It makes a
                                \ word that makes a word.
  state @ if                    \ Compile time behavior
  ( NOTE: ' _create , *nearly* works here )
  ' :: ,                        \ Make the defining word compile a header
  write-quote push-location , compile,    \ Write in push to the creating word
  ' here , ' 3+ , compile,        \ Write in the number we want the created word to push
  write-quote >mark compile,   \ Write in a place holder 0 and push a 
                                     \ pointer to to be used by does>
  write-quote write-exit compile, \ Write in an exit in the word we're compiling.
  ['] command-mode ,           \ Make sure to change the state back to command mode )
  else                         \ Run time behavior
        creater
  then
;
hider creater
hider state! 

: does> ( whole-to-patch -- ) 
  immediate
  write-exit            ( write in an exit, we don't want the
                          defining to run it, but the *defined* word to )
  here swap !           ( patch in the code fields to point to )
  run-instruction ,     ( write a run in )
;

hider write-quote 

: array    ( length --  :create a array )           create allot does> + ;
: variable ( initial-value -- : create a variable ) create ,     does>   ;
: constant ( value -- : crate a constant )          create ,     does> @ ;

: do immediate
	' swap ,      ( compile 'swap' to swap the limit and start )
	' >r ,        ( compile to push the limit onto the return stack )
	' >r ,        ( compile to push the start on the return stack )
	here          ( save this address so we can branch back to it )
;

: addi 
	( TODO: Simplify )
	r@ 1-        ( get the pointer to i )
	+!           ( add value to it )
	r@ 1- @      ( find the value again )
	r@ 2- @      ( find the limit value )
	<
	if
		r@ @ @ r@ @ + r@ ! exit ( branch )
	then
	r> 1+
	rdrop
	rdrop
	>r
;

: loop immediate push-location , 1 , ' addi , here - , ;
: +loop immediate ' addi , here - , ;
hider inci 
hider addi

: leave
	( break out of a do-loop construct )
	rdrop ( pop off our return address )
	rdrop ( pop off i )
	rdrop ( pop off the limit of i and return to the caller's caller routine )
;

: ?leave ( x -- : conditional leave )
	if
		rdrop ( pop off our return address )
		rdrop ( pop off i )
		rdrop ( pop off the limit of i and return to the caller's caller routine )
	then
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

: pick ( xu ... x1 x0 u -- xu ... x1 x0 xu )
	stack-start depth + swap - 1- @
;

: .s    ( -- : print out the stack for debugging )
	depth if
		depth 0 do i pick . tab loop
	then
	cr
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
	' branch ,
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

: alignment-bits [ 1 size log2 lshift 1- literal ] and ;
: name ( PWD -- c-addr ) 
	( given a pointer to the PWD field of a word get a pointer to the name
	of the word )
	dup 1+ @ 256/ lsb - chars>
;


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
			0 swap c! drop
			i 1+
			leave
		then
	loop
	begin ( read until delimiter )
		key delim @ =
	until
;
hider delim 


: '"' ( -- char : push literal " character ) [char] " ;
: accept ( c-addr +n1 -- +n2 : see accepter definition ) swap '\n' accepter ;

255 constant max-string-length 
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
hider delim 

size 1- constant aligner 
: aligned 
	aligner + aligner invert and
;
hider aligner 

( string handling should really be done with PARSE, and CMOVE )

0 variable delim
: write-string ( char -- c-addr u )
	( Write a string into word being currently defined, this
	code has to jump over the string it has just put into the
	dictionary so normal execution of a word can continue. The
	length and character address of the string are left on the
	stack )
	delim !         ( save delimiter )
	' branch ,      ( write in jump, this will jump past the string )
	>mark           ( make hole )
	dup 1+ chars>   ( calculate address to write to )
	max-string-length delim @ accepter dup >r ( write string into dictionary, save index )
	aligned 2dup size / ( stack: length hole char-len hole )
	dup here + h !   ( update dictionary pointer with string length )
	1+ swap !        ( write place to jump to )
	drop             ( do not need string length anymore )
	push-location ,  ( push next value ) 
	1+ chars> dup >r ( calculate place to print, save it )
	,                ( write value to push, the character address to print)
	' print ,        ( write out print, which will print our string )
	r> r> swap       ( restore index and address of string )
;
hider delim

: do-string ( char -- )
	state @ if write-string 2drop else print-string then
;

: "  immediate '"' do-string ;
: .( immediate ')' do-string ;
: ." immediate '"' do-string ;

: ok " ok" cr ;

: count ( c-addr1 -- c-addr2 u ) dup c@ swap 1+ swap ;

: fill ( c-addr u char -- )
	( fill in an area of memory with a character, only if u is greater than zero )
	swap 
	0 do 2dup i + c! loop
	2drop
;

: spaces ( n -- ) 0 do space loop ;
: erase ( addr u )
	chars> swap chars> 0 -rot fill
;

: blank ( c-addr u )
	( given a character address and length, this fills that range with spaces )
	bl -rot swap fill
;

: move ( addr1 addr2 u -- )
	( copy u words of memory from 'addr2' to 'addr1' )
	0 do
		2dup i + @ swap i + !
	loop
	2drop
;

( It would be nice if move and cmove could share more code, as they do exactly
  the same thing but with different load and store functions, cmove>  )
: cmove ( c-addr1 c-addr2 u -- )
	( copy u characters of memory from 'c-addr2' to 'c-addr1' )
	0 do
		2dup i + c@ swap i + c!
	loop
	2drop
;

( The words "[if]", "[else]" and "[then]" implement conditional compilation,
they can be nested as well

See http://lars.nocrew.org/dpans/dpans15.htm for more information

A much simpler conditional compilation method is the following
single word definition:

 : compile-line? 0= if [ find \\ , ] then ;

Which will skip a line if a conditional is false, and compile it
if true )

( These words really, really need refactoring )
0 variable nest      ( level of [if] nesting )
0 variable else-word ( this will be populated with a forward reference to [else] )
: unless immediate ( bool -- : like 'if' but execute clause if false ) ' 0= , postpone if ;
: endif immediate ( synonym for 'then' ) postpone then ;
: [then] immediate ;

: reset-nest 1 nest ! ;
: [if]
	unless
		reset-nest
		begin
			find ( read in a word, we'll try to process it )
			dup [ find [if]   ] literal =     if nest 1+! then
			dup else-word @ = nest @ 1  = and if exit then
			    [ find [then] ] literal =     if nest 1-! then
			nest @ 0=
		until
	then
;

: [else]
	reset-nest
	begin
		find
		dup [ find [if]   ] literal = if nest 1+! then
		    [ find [then] ] literal = if nest 1-! then
		nest @ 0=
	until
;

find [else] else-word ! 
hider else-word
hider nest
hider reset-nest

size 2 = [if] 0x0123           variable endianess [then]
size 4 = [if] 0x01234567       variable endianess [then]
size 8 = [if] 0x01234567abcdef variable endianess [then]

: endian ( -- bool )
	( returns the endianess of the processor )
	[ endianess chars> c@ 0x01 = ] literal 
;
hider endianess

( create a new variable "pad", which can be used as a scratch pad,
  other versions of pad make it relative to the dictionary pointer
  so modifying modifies the pad pointer )
0 variable pad 32 allot

: dump  ( addr u -- )
	( dump out contents of memory starting at 'addr', for 'u' units )
	over + lister 2drop
;

: forget 
	( given a word, forget every word defined after that
	word and the word itself )
	find 
	1- dup @ pwd ! h ! 
;


: ?dup-if immediate ( x -- x | - ) 
	' ?dup , 
	' ?branch , here 0 ,
	( compile if )
;

: ** ( b e -- x : exponent, raise 'b' to the power of 'e')
	dup
	if 
		dup
		1
		do over * loop
	else
		drop
		1
	endif
;

0 variable char-alignment
: c, ( char c-addr -- : write a character into the dictionary )
	char-alignment @ dup 1+ size mod char-alignment !
	+
	c!
;
hider char-alignment

( ANSI Escape Codes )
27 constant 'escape'
char ; constant ';'
: CSI 'escape' emit ." [" ;
0  constant black   
1  constant red     
2  constant green   
3  constant yellow 
4  constant blue     
5  constant magenta  
6  constant cyan     
7  constant white  
: foreground 30 + ;
: background 40 + ;
0 constant dark
1 constant bright

: color ( brightness color-code -- )
	( set color on an ANSI compliant terminal,
	for example:
		bright red foreground color
	sets the foreground text to bright red )
	CSI . 
	if ." ;1" then ." m" 
;

: at-xy ( x y -- ) ( set ANSI terminal cursor position to x y ) CSI . ';' emit . ." H" ;
: page  ( clear ANSI terminal screen and move cursor to beginning ) CSI ." 2J" 1 1 at-xy ;
: hide-cursor ( hide the cursor from view ) CSI ." ?25l" ;
: show-cursor ( show the cursor )           CSI ." ?25h" ;
: save-cursor ( save cursor position ) CSI ." s" ;
: restore-cursor ( restore saved cursor position ) CSI ." u" ;
: reset-color CSI ." 0m" ;

hider CSI

: registers ( -- )
	( print out important registers and information about the
	virtual machine )
	" return stack pointer: " r@       . cr
	" dictionary pointer    " here     . cr
	" previous word:        " pwd   @  . cr
	" state:                " state @  . cr
	" base:                 " base  @  . cr
	" depth:                " depth    . cr
	" cell size (in bytes): " size     . cr
	" last cell address:    " max-core . cr
	" unused cells:         " unused   . cr
	( We could determine if we are reading from stdin by looking
	at the stdin register )
	" current input source: " source-id -1 = if " string" else " file" then cr
	( depth if " Stack: " .s then )
	( " Raw Register Values: " cr 0 31 dump )
;

: y/n? ( -- bool : ask a yes or no question )
	key drop
	" y/n? "
	begin
		key
		dup 
		[char] y = if true  exit then
		[char] n = if false exit then
		" y/n? "
	again
;

: TrueFalse if " true" else " false" then ;
: >instruction ( extract instruction from instruction field ) 0x1f and ;

: word-printer
	( this word expects a pointer to an execution token and must determine
	whether the word is immediate, and whether it has no name, this
	needs to be implemented )
	0
;

: decompile ( code-field-ptr -- )
	( This word expects a pointer to the code field of a word, it
	decompiles a words code field, it needs a lot of work however.
	There are several complications to implementing this decompile
	function, ":noname", "branch", multiple "exit" commands, and literals.

	This word really needs CASE statements before it can be
	completed, as it stands the function is not complete

	:noname  This has a marker before its code field of -1 which
	         cannot occur normally
	branch   branches are used to skip over data, but also for
	         some branch constructs, any data in between can only
	         be printed out generally speaking 
	exit     It might be difficult to determine when a word actually
	         actually stops, one way of fixing this is to write 
		 something after the terminating exit [placed by ';'],
		 such as all bits set, which is not something that
	         can occur normally
	literals Literals can be distinguished by their low value,
	         which cannot possibly be a word with a name, the
	         next field is the actual literal )
	255 0
	do
		tab 
		dup @ push-location = if " literal:" tab 1+ dup @ . 1+ cr then
		dup @ [ find branch execution-token ] literal = if " branch:" tab 1+ dup @ . 1+ cr then
		dup @ [ find exit execution-token ] literal = if " exit" i cr leave  then
		cr
		1+
	loop
	cr
	255 = if " decompile limit exceeded" cr then
	drop
;
hider word-printer

: see 
	( decompile a word )
	find 
	dup dictionary-start < if drop " word not found" cr exit then
	1-
	dup " name:          " name print cr
	dup " word start:    " name chars . cr
	dup " previous word: " @ . cr
	dup " immediate:     " 1+ @ >instruction compile-instruction <> TrueFalse cr
	dup " instruction:   " execution-token @ >instruction . cr ( @todo lookup instruction name )
	dup " defined:       " execution-token @ >instruction run-instruction = dup TrueFalse cr
	if
		execution-token 1+
		" code field:    " cr
		decompile
	else
		drop
	then
;
hider decompile
hider TrueFalse
hider >instruction

( \ Test code

: ' immediate 
	\ create a version of "'" that works at command time as well
        \ as compile time 
	[ 
		hide '      \ hide this definition, we will use the hide token it produces 
		hide ' drop \ hide the built in definition , drop the token it produces 
	]
	state @ 
	if 
		push-location , find execution-token , 
	else 
		find 
	then 
	[ unhide ] 
;

0x8000              constant hello-buffer
hello-buffer chars  constant move-1-buffer
move-1-buffer 0x100 + constant move-2-buffer


255 hello-buffer accept hello world
drop

move-2-buffer move-1-buffer 40 move

\ ." Dump:" move-1-buffer cr move-1-buffer 20 dump
\ ." Dump:" move-2-buffer cr move-2-buffer 20 dump

move-1-buffer chars> print cr
move-2-buffer chars> print cr

move-2-buffer 40 erase
move-1-buffer chars> print cr
move-2-buffer chars> print cr 
\ move-2-buffer 40 char a fill \ fill needs fixing
move-2-buffer chars> print cr 

: aword            " example non immediate word to execute " cr ; find aword  execute
: aiword immediate " example immediate word to execute " cr ;     find aiword execute
2 2 find + execute . cr
:noname " hello world " cr ;  execute

1 [if] 
	." Hello World " 
	0 [if]
		." Bye, bye! " cr
	[else]

		0 [if]
			." Good bye!" cr
		[else]
			." Good bye, cruel World!" cr
		[then]
	[then]


[then] 
: c 99  ?dup-if 2 then .s ; 
find c 16 dump
cr
c 

: make-star     [char] * emit ;
: make-stars    0 do make-star loop cr ;
: make-square   dup 0 do dup make-stars loop drop ;
: make-triangle 1 do i make-stars loop ;
: make-tower    dup make-triangle make-square ;

: memcpy 
  over +
  do
    dup @ @ swap ! 1+
  loop
  drop
;

: memset 
  rot dup rot +
  do
    dup @ !
  loop
;

." constant " cr
31 constant a
a . cr

)

hider write-string 
hider do-string 
hider  accept-string        
hider  '"'                  
hider  ')'                  
hider  alignment-bits       
hider  print-string         
hider  compile-instruction  
hider  dictionary-start     
hider  hidden?              
hider  hidden-mask          
hider  instruction-mask     
hider  max-core             
hider  push-location        
hider  run-instruction      
hider  stack-start          
hider  x                    
hider  x!                   
hider  x@                   
hider  write-exit           
hider  branch
hider  ?branch
hider  mask-byte            
hider  accepter             
hider  max-string-length    
hider  line                 
hider  source-id-reg        
hider  execution-token 

( TODO
	* Evaluate 
	* By adding "c@" and "c!" to the interpreter I could remove print
	* Add unit testing to the end of this file
	* Word, Parse, repeat, while, Case statements, other forth words
	* Try to simplify the definitions of write-string using "create" "does>"
	and come up with a way of reusing code for "move", "cmove" and "cmove>",
	* Add a dump core and load core to the interpreter?
	* add "j" if possible to get outer loop context
	* Decompiler "see" http://lars.nocrew.org/dpans/dpans15.htm
	* FORTH, VOCABULARY 
	* "Value", "To", "Is"
	* Check "fill", "erase", "'", other words
	* Make signed comparison operations
)

