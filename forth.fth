#!./forth -t
( Welcome to libforth, A dialect of Forth. Like all versions of Forth this 
version is  a little idiosyncratic, but how the interpreter works is
documented here and in various other files.

This file contains most of the start up code, some basic start up code
is executed in the C file as well which makes programming at least bearable.
Most of Forth is programmed in itself, which may seem odd if your back
ground in programming comes from more traditional language [such as C], 
although decidedly less so if you know already know lisp, for example.

For more information about this interpreter and Forth see: 
	https://en.wikipedia.org/wiki/Forth_%28programming_language%29
	readme.md   : for a manual for this interpreter
	libforth.h  : for information about the C API
	libforth.3  : for limited information about the C API
	libforth.c  : for the interpreter itself
	unit.c      : a series of unit tests against libforth.c
	unit.fth    : a series of unit tests against this file

The interpreter and this code originally descend from a Forth interpreter
written in 1992 for the International obfuscated C Coding Competition 

See:
	http://www.ioccc.org/1992/buzzard.2.design


TODO
	* This file should be made to be literate

	* Word, Parse, other forth words
	* add "j" if possible to get outer loop context
	* FORTH, VOCABULARY 
	* "Value", "To", "Is"
	* The interpreter should use character based addresses, instead of
	word based, and use values that are actual valid pointers, this
	will allow easier interaction with the world outside the virtual machine
	* A small block editor
	* more help commands would be good, such as "help-ANSI-escape",
	"tutorial", etc.
	* Abort", this could be used to implement words such
	as "abort if in compile mode", or "abort if in command mode".
	* Todo Various different assemblers [assemble VM instructions,
	native instructions, and cross assemblers]
	* common words and actions should be factored out to simplify
	definitions of other words, their standards compliant version found
	if any
	* throw and exception
	* here documents, string literals
	* A set of words for navigating around word definitions would be
	help debugging words, for example:
		compile-field code-field field-translate
	would take a pointer to a compile field for a word and translate
	that into the code field
	* proper booleans should be used throughout
	* escaped strings
	* ideally all of the functionality of main_forth would be
	moved into this file
	* fill move and the like are technically not compliant as
	they do not test if the number is negative, however that would
	unnecessarily limit the range of operation
	* "print" should be removed from the interpreter
	* :inline ; to inline a words
	* words should be made for debugging the return stack [such as
	printing it out like .s]

Some interesting links:
	* http://www.figuk.plus.com/build/heart.htm
	* https://groups.google.com/forum/#!msg/comp.lang.forth/NS2icrCj1jQ/1btBCkOWr9wJ
	* http://newsgroups.derkeiler.com/Archive/Comp/comp.lang.forth/2005-09/msg00337.html
	* https://stackoverflow.com/questions/407987/what-are-the-primitive-forth-operators
)

( ========================== Basic Word Set ================================== )
( We'll begin by defining very simple words we can use later )
: 1+ 1 + ;
: 1- 1 - ;
: tab 9 emit ;
: 0= 0 = ; 
: not 0= ; 
: <> = 0= ;
: < u< ; ( @todo fix this )
: > u> ; ( @todo fix this )
: logical ( x -- bool ) not not ;
: :: [ find : , ] ;
: '\n' 10 ;
: cr '\n' emit ;
: hidden-mask 0x80 ( Mask for the hide bit in a words MISC field ) ;
: instruction-mask 0x1f ( Mask for the first code word in a words MISC field ) ;
: hidden? ( PWD -- PWD bool ) dup 1+ @ hidden-mask and logical ;
: compile-instruction 1 ; ( compile code word, threaded code interpreter instruction )
: dolist 2 ;      ( run code word, threaded code interpreter instruction )
: dolit 2 ;       ( location of special "push" word )
: [literal] dolit , , ; ( this word probably needs a better name )
: literal immediate [literal] ;
: latest pwd @ ; ( get latest defined word )
: false 0 ;
: true 1 ;
: *+ * + ;
: 2- 2 - ;
: 2+ 2 + ;
: 3+ 3 + ;
: 2* 1 lshift ;
: 2/ 1 rshift ;
: 4* 2 lshift ;
: 4/ 2 rshift ;
: 8* 3 lshift ;
: 8/ 3 rshift ;
: 256/ 8 rshift ;
: 2dup over over ;
: mod ( x u -- remainder ) 2dup / * - ;
: */ ( n1 n2 n3 -- n4 ) * / ; ( warning: this does not use a double cell for the multiply )
: char key drop key ;
: [char] immediate char [literal] ;
: postpone immediate find , ;
: unless immediate ( bool -- : like 'if' but execute clause if false ) ' 0= , postpone if ;
: endif immediate ( synonym for 'then' ) postpone then ;
: address-unit-bits size 8* ;
: negative? ( x -- bool : is a number negative? )
	[ 1 address-unit-bits 1- lshift ] literal and logical ;
: mask-byte ( x -- x ) 8* 0xff swap lshift ;
: select-byte ( u i -- c ) 8* rshift 0xFF and ;
: cell+ 1+ ( a-addr1 -- a-addr2 ) ;
: cells immediate ( n1 -- n2 ) ;
: cell 1 ( -- u : 1 cells ) ;
: char+ ( c-addr1 -- c-addr2 ) 1+ ;
: chars  ( n1 -- n2: convert character address to cell address ) size / ;
: 2chars ( x y -- x y: ) chars swap chars swap ;
: chars> ( n1 -- n2: convert cell address to character address ) size * ;
: 2chars> chars> swap chars> swap ;
: hex     ( -- ) ( print out hex )     16 base ! ;
: octal   ( -- ) ( print out octal )    8 base ! ;
: decimal ( -- ) ( print out decimal )  0 base ! ;
: negate ( x -- x ) -1 * ;
: square ( x -- x ) dup * ;
: drup ( x y -- x x ) drop dup ;
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
: 2@ dup 1+ @ swap @ ;
: r@ r> r @ swap >r ;
: 0> 0 > ;
: 0<> 0 <> ;
: nand ( x x -- x : Logical NAND ) and not ;
: odd 1 and ;
: even odd not ;
: nor  ( x x -- x : Logical NOR  )  or not ;
: ms ( u -- : wait at least 'u' milliseconds ) clock +  begin dup clock < until drop ;
: sleep 1000 * ms ;
: align immediate ( x -- x ) ; ( nop in this implementation )
: ) immediate ;
: ? ( a-addr -- : view value at address ) @ . ;
: bell 7 ( ASCII BEL ) emit ;
: b/buf  ( bytes per buffer ) 1024 ;
: # ( x -- x : debug print ) dup . ;
: compile, ' , , ;   ( A word that writes , into the dictionary )
: >mark ( -- ) here 0 , ; 
: <resolve here - , ;
: end immediate (  A synonym for until ) postpone until ;
: bye ( -- : quit the interpreter ) 0 r ! ;
: stack-start [ max-core `stack-size @ 2 * -  ] literal ;
: pick ( xu ... x1 x0 u -- xu ... x1 x0 xu )
	stack-start depth + swap - 1- @ ;
: within ( test low high -- flag : is test between low and high )
	over - >r - r> u< ;
: u. ( u -- : display number in base 10, although signed for now ) 
	base @ >r decimal pnum r> base ! ;
: invalidate-forth
	1 `invalid ! ;
: rdrop ( R: x -- )
	r>           ( get caller's return address )
	r>           ( get value to drop )
	drop         ( drop it like it's hot )
	>r           ( return return address )
;

: again immediate 
	( loop unconditionally in a begin-loop:
		begin ... again )
	' branch , <resolve ; 

( begin...while...repeat These are defined in a very "Forth" way )
: while immediate postpone if ( branch to repeats 'then') ;
: repeat immediate
	swap            ( swap 'begin' here and 'while' here )
	postpone again  ( again jumps to begin )
	postpone then ; ( then writes to the hole made in if )

: never ( never...then : reserve space in word ) 
	immediate 0 [literal] postpone if ;

: dictionary-start 
	( The dictionary start at this location, anything before this value 
	is not a defined word ) 
	64 ;

: source ( -- c-addr u ) 
	( TODO: read registers instead )
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
	-1       String
	0        File Input [this may be stdin] )
	`source-id @ ;

: 2drop ( x y -- ) drop drop ;
: hide  ( token -- hide-token ) 
	( This hides a word from being found by the interpreter )
	dup 
	if 
		dup @ hidden-mask or swap tuck ! 
	else 
		drop 0 
	then ;

: hider ( WORD -- ) ( hide with drop ) find dup if hide then drop ;
: unhide ( hide-token -- ) dup @ hidden-mask invert and swap ! ;

: original-exit [ find exit ] literal ;
: exit 
	( this will define a second version of exit, ';' will
	use the original version, whilst everything else will
	use this version, allowing us to distinguish between
	the end of a word definition and an early exit by other
	means in "see" )
	[ find exit hide ] rdrop exit [ unhide ] ;

: ?exit ( x -- ) ( exit current definition if not zero ) if rdrop exit then ;

: number? ( c -- f : is character a number? )
	[char] 0 [ char 9 1+ ] literal within ;

: lowercase? ( c -- f : is character lower case? )
	[char] a [ char z 1+ ] literal within ;

: uppercase? ( C -- f : is character upper case? )
	[char] A [ char Z 1+ ] literal within ;

: alpha? ( C -- f : is character part of the alphabet? )
	dup lowercase? swap uppercase? or ;

: alphanumeric? ( C -- f : is character alphabetic or a number ? )
	dup alpha? swap number? or ;

: printable? ( c -- bool : is printable, excluding new lines and tables )
	32 127 within ;

( ========================== Basic Word Set ================================== )

( ========================== Extended Word Set =============================== )
: gcd ( a b -- n ) ( greatest common divisor )
	begin
		dup
		if
			tuck mod 0
		else
			1
		then
	until
	drop ;

: log2 ( x -- log2 ) 
	( Computes the binary integer logarithm of a number,
	zero however returns itself instead of reporting an error )
	0 swap 
	begin 
		swap 1+ swap 2/ dup 0= 
	until 
	drop 1- ;

: xt-token ( previous-word-address -- xt-token )
	( Given the address of the PWD field of a word this
	function will return an execution token for the word )
	1+    ( MISC field )
	dup 
	@     ( Contents of MISC field )
	instruction-mask and  ( Mask off the instruction )
	( If the word is not an immediate word, execution token pointer ) 
	compile-instruction = + ;

: ['] immediate find xt-token [literal] ;

: execute ( xt-token -- )
	( given an execution token, execute the word )

	( create a word that pushes the address of a hole to write to
	a literal takes up two words, '!' takes up one )
	1- ( execution token expects pointer to PWD field, it does not
		care about that field however, and increments past it )
	xt-token
	[ here 3+ literal ] 
	!                   ( write an execution token to a hole )
	[ 0 , ]             ( this is the hole we write )
;

: time ( " ccc" -- n : time the number of milliseconds it takes to execute a word )
	clock >r
	find execute
	clock r> - ;

: rdepth 
	max-core `stack-size @ - r @ swap - ;

( ========================== Extended Word Set =============================== )


( ========================== CREATE DOES> ==================================== )

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

: command-mode-create   ( create a new work that pushes its data field )
	::              ( compile a word )
	dolit , ( write push into new word )
	here 2+ ,       ( push a pointer to data field )
	['] exit ,      ( write in an exit to new word data field is after exit )
	command-mode    ( return to command mode )
;

: <build immediate
	( @note ' command-mode-create , *nearly* works )
	' :: ,                               ( Make the defining word compile a header )
	write-quote dolit , compile, ( Write in push to the creating word )
	' here , ' 3+ , compile,             ( Write in the number we want the created word to push )
	write-quote >mark compile,           ( Write in a place holder 0 and push a pointer to to be used by does> )
	write-quote write-exit compile,      ( Write in an exit in the word we're compiling. )
	['] command-mode ,                   ( Make sure to change the state back to command mode )
;

: create immediate  ( create word is quite a complex forth word )
  state @ if postpone <build else command-mode-create then ;

hider command-mode-create
hider state! 

: does> ( whole-to-patch -- ) 
	immediate
	write-exit  ( we don't want the defining to exit, but the *defined* word to )
	here swap !           ( patch in the code fields to point to )
	dolist ,     ( write a run in )
;

: >body ( xt -- a-addr : a-addr is data field of a CREATEd word )
	xt-token 5 + ;
hider write-quote 

( ========================== CREATE DOES> ==================================== )

: limit ( x min max -- x : limit x with a minimum and maximum ) 
	rot min max ;

: array     create allot does> + ;
: table     create allot does>   ;
: variable  create ,     does>   ;
: constant  create ,     does> @ ;
( @todo replace all instances of table with itable )
: itable     create dup , allot does> dup @ ;
: char-table create dup , chars allot does> dup @ swap 1+ chars> swap ;

( do...loop could be improved by not using the return stack so much )

: do immediate
	' swap ,         ( compile 'swap' to swap the limit and start )
	' >r ,           ( compile to push the limit onto the return stack )
	' >r ,           ( compile to push the start on the return stack )
	postpone begin ; ( save this address so we can branch back to it )

: addi 
	( @todo simplify )
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
	>r ;

: loop immediate 1 [literal] ' addi , <resolve ;
: +loop immediate ' addi , <resolve ;
hider inci 
hider addi

: leave ( break out of a do-loop construct )
	rdrop ( pop off our return address )
	rdrop ( pop off i )
	rdrop ( pop off the limit of i and return to the caller's caller routine )
;

: ?leave ( x -- : conditional leave )
	if
		rdrop ( pop off our return address )
		rdrop ( pop off i )
		rdrop ( pop off the limit of i and return to the caller's caller routine )
	then ;

: i ( -- i ) 
	( Get current, or innermost, loop index in do-loop construct )
	r> r> ( pop off return address and i ) 
	tuck  ( tuck i away ) 
	>r >r ( restore return stack ) 
;

: range    ( nX nY -- nX nX+1 ... nY )  swap 1+ swap do i loop ;
: repeater ( n0 X -- n0 ... nX )        1 do dup loop ;
: sum      ( n0 ... nX X -- sum<0..X> ) 1 do + loop ;
: mul      ( n0 ... nX X -- mul<0..X> ) 1 do * loop ;

: factorial ( n -- n! )
	( This factorial is only here to test range, mul, do and loop )
	dup 1 <= 
	if
		drop
		1
	else ( This is obviously super space inefficient )
 		dup >r 1 range r> mul
	then ;

hider tail
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
	latest xt-token 
	' branch ,
	here - 1+ , ;

: recurse immediate
	( This function implements recursion, although this interpreter
	allows calling a word directly. If used incorrectly this will
	blow up the return stack.

	We can test "recurse" with this factorial function:
	  : factorial  dup 2 < if drop 1 exit then dup 1- recurse * ;)
	latest xt-token , ;
: myself immediate postpone recurse ;

0 variable column-counter
4 variable column-width
: column ( i -- )	column-width @ mod not if cr then ;
: reset-column		0 column-counter ! ;
: auto-column		column-counter dup @ column 1+! ;

: alignment-bits [ 1 size log2 lshift 1- literal ] and ;
: name ( PWD -- c-addr ) 
	( given a pointer to the PWD field of a word get a pointer to the name
	of the word )
	dup 1+ @ 256/ lsb - chars> ;

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

: roll 
	( xu xu-1 ... x0 u -- xu-1 ... x0 xu )
	( remove u and rotate u+1 items on the top of the stack,
	this could be replaced with a move on the stack and
	some magic so the return stack is used less )
	dup 0 > 
	if 
		swap >r 1- roll r> swap 
	else 
		drop 
	then ;

: accumulator  ( " ccc" -- : make a word that increments by a value and pushes the result )
	create , does> tuck +! @ ;

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
	until ;
hider delim 

: accept ( c-addr +n1 -- +n2 : see accepter definition ) '\n' accepter ;

0xFFFF constant max-string-length 

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
	until ;
hider delim 

size 1- constant aligner 
: aligned ( unaligned -- aligned : align a pointer )
	aligner + aligner invert and ;
hider aligner 

0 variable delim
: write-string ( char -- c-addr u )
	( @todo This really needs simplifying, to do this
	a set of words that operate on a temporary buffer can
	be used )
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
	1+ chars>        ( calculate place to print )
	r>               ( restore index and address of string )
;
hider delim

: length ( c-addr u -- u : )
  tuck 0 do dup c@ 0= if 2drop i leave then 1+ loop ;

: asciiz?
	tuck length <> ;

: asciiz
	2dup length nip ;

: type ( c-addr u -- : print out 'u' characters at c-addr )
	0 do dup c@ emit 1+ loop drop ;

: do-string ( char -- )
	state @ if write-string swap [literal] [literal] ' type , else print-string then ;

: c" immediate [char] " write-string ;

: fill ( c-addr u char -- )
	( fill in an area of memory with a character, only if u is greater than zero )
	-rot
	0 do 2dup i + c! loop
	2drop ;

128 char-table sbuf
: s" ( "ccc<quote>" --, Run Time -- c-addr u ) 
	sbuf 0 fill sbuf [char] " accepter sbuf drop swap ;
hider sbuf

: "  immediate [char] " do-string ;
: .( immediate [char] ) do-string ;
: ." immediate [char] " do-string ;

: ok " ok" cr ;

( ==================== CASE statements ======================== )

( for a simpler case statement: 
	see Volume 2, issue 3, page 48 of Forth Dimensions at 
	http://www.forth.org/fd/contents.html )

( These case statements need improving )
: case immediate 
	' branch , 3 ,   ( branch over the next branch )
	here ' branch ,  ( mark: place endof branches back to with again )
	>mark swap ;     ( mark: place endcase writes jump to with then )

: over= over = ; 
: of      immediate ' over= , postpone if ;
: endof   immediate over postpone again postpone then ;
: endcase immediate 1+ postpone then drop ;

( ==================== CASE statements ======================== )

: error-no-word ( print error indicating last read in word as source )
	"  error: word '" source drop print " ' not found" cr ;

: ;hide ( should only be matched with ':hide' ) 
	immediate " error: ';hide' without ':hide'" cr ;

: :hide
	( hide a list of words, the list is terminated with ";hide" )
	begin
		find ( find next word )
		dup [ find ;hide ] literal = if 
			drop exit ( terminate :hide )
		then
		dup 0= if ( word not found )
			drop 
			error-no-word
			exit 
		then
		hide drop
	again ;

: count ( c-addr1 -- c-addr2 u ) dup c@ swap 1+ swap ;

: bounds ( x y -- y+x x ) 
	over + swap ;

: spaces ( n -- : print n spaces ) 0 do space loop ;
: erase ( addr u : erase a block of memory )
	2chars> 0 fill ;

: blank ( c-addr u )
	( given a character address and length, this fills that range with spaces )
	bl fill ;

: move ( addr1 addr2 u -- )
	( copy u words of memory from 'addr2' to 'addr1' )
	0 do
		2dup i + @ swap i + !
	loop
	2drop ;

( It would be nice if move and cmove could share more code, as they do exactly
  the same thing but with different load and store functions, cmove>  )
: cmove ( c-addr1 c-addr2 u -- )
	( copy u characters of memory from 'c-addr2' to 'c-addr1' )
	0 do
		2dup i + c@ swap i + c!
	loop
	2drop ;

( The words "[if]", "[else]" and "[then]" implement conditional compilation,
they can be nested as well

See http://lars.nocrew.org/dpans/dpans15.htm for more information

A much simpler conditional compilation method is the following
single word definition:

 : compile-line? 0= if [ find \\ , ] then ;

Which will skip a line if a conditional is false, and compile it
if true )

( These words really, really need refactoring )
0 variable      nest        ( level of [if] nesting )
0 variable      [if]-word    ( populated later with "find [if]" )
0 variable      [else]-word  ( populated later with "find [else]")
: [then]        immediate ; 
: reset-nest    1 nest ! ;
: unnest?       [ find [then] ] literal = if nest 1-! then ;
: nest?         [if]-word             @ = if nest 1+! then ;
: end-nest?     nest @ 0= ;
: match-[else]? [else]-word @ = nest @ 1 = and ; 

: [if] ( bool -- : conditional execution )
	unless
		reset-nest
		begin
			find 
			dup nest?
			dup match-[else]? if drop exit then
			    unnest?
			end-nest?
		until
	then ;

: [else] ( discard input until [then] encounter, nesting for [if] )
	reset-nest
	begin
		find
		dup nest? unnest?
		end-nest?
	until ;

find [if] [if]-word !
find [else] [else]-word ! 

:hide [if]-word [else]-word nest reset-nest unnest? match-[else]? ;hide

size 2 = [if] 0x0123           variable endianess [then]
size 4 = [if] 0x01234567       variable endianess [then]
size 8 = [if] 0x01234567abcdef variable endianess [then]

: endian ( -- bool )
	( returns the endianess of the processor )
	[ endianess chars> c@ 0x01 = ] literal 
;
hider endianess

: evaluate ( c-addr u -- : evaluate a NUL terminated string )
	drop evaluator ;

: trace ( flag -- : turn tracing on/off ) 
	`debug ! ;

: pad 
	( the pad is used for temporary storage, and moves
	along with dictionary pointer, always in front of it )
	here 64 + ;

0 variable counter
: counted-column ( index -- )
	( special column printing for dump )
	counter @ column-width @ mod 
	not if cr . " :" space else drop then
	counter 1+! ;


: as-chars ( x -- : print a cell out as characters )
	size 0 
	do 
		dup 
		size i 1+ - select-byte ( @todo adjust for endianess ) 
		dup printable? not
		if 
			drop [char] . 
		then 
		emit
	loop 
	space
	drop
;

: lister 0 counter ! swap do i counted-column i ? i @ as-chars loop ;

: dump  ( addr u -- : dump out 'u' cells of memory starting from 'addr' )
	base @ >r hex 1+ over + lister r> base ! cr ;
:hide counted-column counter lister as-chars ;hide

: forgetter ( pwd-token -- : forget a found word and everything after it )
	dup @ pwd ! h ! ;

( @bug will not work for immediate defined words )
: forget ( WORD -- : forget word and every word defined after it )
	find 1- forgetter ;

: marker ( WORD -- : make word the forgets itself and words after it)
	:: latest [literal] ' forgetter , postpone ; ;
hider forgetter

: ?dup-if immediate ( x -- x | - ) 
	' ?dup , postpone if ;

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

( ==================== Random Numbers ========================= )

( See: 
	uses xorshift
	https://en.wikipedia.org/wiki/Xorshift
	http://excamera.com/sphinx/article-xorshift.html
	http://www.arklyffe.com/main/2010/08/29/xorshift-pseudorandom-number-generator/
)
( these constants have be collected from the web )
size 2 = [if] 13 constant a 9  constant b 7  constant c [then]
size 4 = [if] 13 constant a 17 constant b 5  constant c [then]
size 8 = [if] 12 constant a 25 constant b 27 constant c [then]

7 variable seed ( must not be zero )

: seed! ( u -- : set the value of the PRNG seed )
	dup 0= if drop 7 ( zero not allowed ) then seed ! ;

: random
	( assumes word size is 32 bit )
	seed @ 
	dup a lshift xor
	dup b rshift xor
	dup c lshift xor
	dup seed! ;
:hide a b c seed ;hide

( ==================== Random Numbers ========================= )

( ==================== ANSI Escape Codes ====================== )
(
	see: https://en.wikipedia.org/wiki/ANSI_escape_code 
	These codes will provide a relatively portable means of 
	manipulating a terminal

	@bug won't work if hex is set
)

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
	CSI u. if ." ;1" then ." m" ;

: at-xy ( x y -- : set ANSI terminal cursor position to x y ) 
	CSI u. ';' emit u. ." H" ;
: page  ( clear ANSI terminal screen and move cursor to beginning ) CSI ." 2J" 1 1 at-xy ;
: hide-cursor ( hide the cursor from view ) CSI ." ?25l" ;
: show-cursor ( show the cursor )           CSI ." ?25h" ;
: save-cursor ( save cursor position ) CSI ." s" ;
: restore-cursor ( restore saved cursor position ) CSI ." u" ;
: reset-color CSI ." 0m" ;
hider CSI
( ==================== ANSI Escape Codes ====================== )


( ==================== Prime Numbers ========================== )
( from original "third" code from the ioccc http://www.ioccc.org/1992/buzzard.2.design )
: prime? ( x -- x/0 : return number if it is prime, zero otherwise )
	dup 1 = if 1- exit then
	dup 2 = if    exit then
	dup 2 / 2     ( loop from 2 to n/2 )
	do
		dup   ( value to check if prime )
		i mod ( mod by divisor )
		not if
			drop 0 leave
		then
	loop
;

0 variable counter
: primes
	0 counter !
	"  The primes from " dup . " to " over . " are: "
	cr
	reset-column
	do
		i prime?
		if
			i . counter @ column counter 1+!
		then
	loop
	cr
	"  There are " counter @ . " primes."
	cr ;
hider counter
( ==================== Prime Numbers ========================== )

( ==================== Debugging info ========================= )

( string handling should really be done with PARSE, and CMOVE )
: .s    ( -- : print out the stack for debugging )
	" <" depth u. " >" space
	depth if
		depth 0 do i column tab depth i 1+ - pick u. loop
	then
	cr ;

1 variable hide-words ( do we want to hide hidden words or not )
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
	latest 
	begin 
		dup 
		hidden? hide-words @ and
		not if 
			name
			print space
		else 
			drop 
		then 
		@  ( Get pointer to previous word )
		dup dictionary-start < ( stop if pwd no longer points to a word )
	until 
	drop cr 
; 
hider hide-words

: TrueFalse if " true" else " false" then ;

: registers ( -- )
	( print out important registers and information about the
	virtual machine )
	" return stack pointer:    " r@       . cr
	" dictionary pointer       " here     . cr
	" previous word:           " pwd      ? cr
	" state:                   " state    ? cr
	" base:                    " base     ? cr
	" depth:                   " depth    . cr
	" cell size (in bytes):    " size     . cr
	" last cell address:       " max-core . cr
	" unused cells:            " unused   . cr
	" invalid:                 " `invalid @ TrueFalse cr
	" size of variable stack:  " `stack-size ? cr
	" size of return stack:    " `stack-size ? cr
	" start of variable stack: " max-core `stack-size @ 2* - . cr
	" start of return stack:   " max-core `stack-size @ - . cr
	" current input source:    " source-id -1 = if " string" else " file" then cr
	" reading from stdin:      " source-id 0 = `stdin @ `fin @ = and TrueFalse cr 
	" tracing on:              " `debug   @ TrueFalse cr
	" starting word:           " `instruction ? cr
	" real start address:      " `start-address ? cr
( 
 `sin `sidx `slen `fout 
 `stdout `stderr `argc `argv `start-time )
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
	again ;

: >instruction ( extract instruction from instruction field ) 0x1f and ;

: step
	( step through a word: this word could be augmented
	with commands such as "dump", "halt", and optional
	".s" and "registers" )
	registers
	" .s: " .s cr
	" -- press any key to continue -- "
	key drop ;

: more ( wait for more input )
	"  -- press any key to continue -- " key drop cr ;

( this is not quite ready for prime time )
: debug-help " debug mode commands 
	h - print help
	q - exit containing word
	r - print registers
	s - print stack
	c - continue on with execution 
" ;
: debug-prompt ." debug> " ;
: debug ( a work in progress, debugging support, needs parse-word )
	key drop
	cr
	begin
		debug-prompt
		key dup '\n' <> if source accept drop then
		case
			[char] h of debug-help endof
			[char] q of bye        endof
			[char] r of registers  endof
			\ [char] d of dump endof \ implement read in number
			[char] s of >r .s r>   endof 
			[char] c of drop exit  endof
		endcase drop
	again ;
hider debug-prompt

0 variable cf
: code>pwd ( CODE -- PWD/0 )
	( @todo simplify using "within"
	 given a pointer to a executable code field
	this words attempts to find the PWD field for
	that word, or return zero )
	dup dictionary-start here within not if drop 0 exit then
	cf !
	latest dup @ ( p1 p2 )
	begin
		over ( p1 p2 p1 )
		cf @ <= swap cf @ > and if exit then
		dup 0=                  if exit then 
		dup @ swap 
	again
;
hider cf

: end-print ( x -- ) "		=> " . " ]" ;
: word-printer
	( attempt to print out a word given a words code field
	WARNING: This is a dirty hack at the moment
	NOTE: given a pointer to somewhere in a word it is possible
	to work out the PWD by looping through the dictionary to
	find the PWD below it )
	1- dup @ -1 =              if " [ noname" end-print exit then
	   dup  " [ " code>pwd dup if name print else drop " data" then 
	        end-print ;
hider end-print

( these words push the execution tokens for various special cases for decompilation )
: get-branch  [ find branch xt-token ]  literal ;
: get-?branch [ find ?branch xt-token ] literal ;
: get-original-exit [ original-exit xt-token ] literal ;
: get-quote   [ find ' xt-token ] literal ;

: branch-increment ( addr branch -- increment : calculate decompile increment for "branch" )
	1+ dup negative? if drop 2 else 2dup dump then ;

( these words take a code field to a primitive they implement, decompile it
and any data belonging to that operation, and push a number to increment the
decompilers code stream pointer by )
: decompile-literal ( code -- increment ) 
	" [ literal	=> " 1+ ? " ]" 2 ;
: decompile-branch  ( code -- increment ) 
	" [ branch	=> " 1+ ? " ]" dup 1+ @ branch-increment ;
: decompile-quote   ( code -- increment ) 
	" [ '	=> " 1+ @ word-printer "  ]" 2 ;
: decompile-?branch ( code -- increment ) 
	" [ ?branch	=> " 1+ ? " ]" 2 ;

: decompile ( code-field-ptr -- )
	( @todo decompile :noname, make the output look better
 
	This word expects a pointer to the code field of a word, it
	decompiles a words code field, it needs a lot of work however.
	There are several complications to implementing this decompile
	function.

	'        The next cell should be pushed 
	:noname  This has a marker before its code field of -1 which
	         cannot occur normally, this is handles in word-printer
	branch   branches are used to skip over data, but also for
	         some branch constructs, any data in between can only
	         be printed out generally speaking 
	exit     There are two definitions of exit, the one used in
		 ';' and the one everything else uses, this is used
		 to determine the actual end of the word
	literals Literals can be distinguished by their low value,
	         which cannot possibly be a word with a name, the
	         next field is the actual literal 

	Of special difficult is processing 'if' 'else' 'then' statements,
	this will require keeping track of '?branch'.

	Also of note, a number greater than "here" must be data )
	begin
		tab 
		dup @
		case
			dolit             of drup decompile-literal endof
			get-branch        of drup decompile-branch  endof 
			get-quote         of drup decompile-quote   endof
			get-?branch       of drup decompile-?branch endof
			get-original-exit of 2drop " [ exit ]" cr exit  endof
			word-printer 1
		endcase 
		+
		cr 
	again ;
:hide 
	word-printer get-branch get-?branch get-original-exit get-quote branch-increment 
	decompile-literal decompile-branch decompile-?branch decompile-quote
;hide

: xt-instruction ( extract instruction from execution token ) 
	xt-token @ >instruction ;
( these words expect a pointer to the PWD field of a word )
: defined-word?      xt-instruction dolist = ;
: print-name         " name:          " name print cr ;
: print-start        " word start:    " name chars . cr ;
: print-previous     " previous word: " @ . cr ;
: print-immediate    " immediate:     " 1+ @ >instruction compile-instruction <> TrueFalse cr ;
: print-instruction  " instruction:   " xt-instruction . cr ;
: print-defined      " defined:       " defined-word? TrueFalse cr ;

: print-header ( PWD -- is-immediate-word? )
	dup print-name
	dup print-start
	dup print-previous
	dup print-immediate
	dup print-instruction ( TODO look up instruction name )
	print-defined ;

: see 
	( decompile a word )
	find 
	dup 0= if drop error-no-word exit then
	1- ( move to PWD field )
	dup print-header
	dup defined-word?
	if ( decompile if a compiled word )
		xt-token 1+ ( move to code field )
		" code field:" cr
		decompile
	else ( the instruction describes the word if it is not a compiled word )
		drop
	then ;

: empty-stack ( x-n ... x-0 -- : empty the variable stack )
	begin depth while drop repeat ;

r @ constant restart-address
: quit
	0 `source-id !  ( set source to read from file )
	`stdin @ `fin ! ( read from stdin )
	postpone [      ( back into command mode )
	restart-address r ! ( restart interpreter; this needs improving ) ;

: abort 
	empty-stack quit ;

( These help messages could be moved to blocks, the blocks could then
  be loaded from disk and printed instead of defining the help here,
  this would allow much larger help )
: help ( print out a short help message )
	page
	key drop
" Welcome to Forth, an imperative stack based language. It is both a low
level and a high level language, with a very small memory footprint. Most
of Forth is defined as a combination of various primitives.

A short description of the available function (or Forth words) follows,
words marked (1) are immediate and cannot be used in command mode, words
marked with (2) define new words. Words marked with (3) have both command
and compile functionality.

" 
more " The built in words that accessible are:

(1,2)	:                 define a new word, switching to compile mode
	immediate         make latest defined word immediate
	read              read in a word, execute in command mode else compile 
	@ !               fetch, store
	c@ c!             character based fetch and store
	- + * /           standard arithmetic operations,
	and or xor invert standard bit operations
	lshift rshift     left and right bit shift
	< > =             comparison predicates
	exit              exit from a word
	emit              print character from top of stack
	key               get a character from input
	r> >r             pop a value from or to the return stack
	find              find a word in the dictionary and push the location
	'                 store the address of the following word on the stack
	,                 write the top of the stack to the dictionary
	bsave bload       save or load a block at address to indexed file
	swap              swap first two values on the stack
	dup               duplicate the top of the stack
	drop              pop and drop a value
	over              copy the second stack value over the first
	.                 pop the top of the stack and print it
	print             print a NUL terminated string at a character address
	depth             get the current stack depth
	clock             get the time since execution start in milliseconds
	evaluator         evaluate a string
	system            execute a system command

 "

more " All of the other words in the interpreter are built from these
primitive words. A few examples:

(1)	if...else...then  FORTH branching construct 
(1)	begin...until     loop until top of stack is non zero
(1)	begin...again     infinite loop
(1)	do...loop         FORTH looping construct
(2,3)	create            create a new word that pushes its location
(1)	does>             declare a created words run time behaviour
(1,2)	variable          declare variable with initial value from top of stack
(1,2)	constant          declare a constant, taken from top of stack
(1,2)	array             declare an array with size taken from top of stack
(1)	;                 terminate a word definition and return to command mode
	words             print out a list of all the defined words
	help              this help message
	dump              print out memory contents starting at an address
	registers         print out the contents of the registers
	see               decompile a word, viewing what words compose it
	.s                print out the contents of the stack

"

more " Some more advanced words:

	here              push the dictionary pointer
	h                 push the address of the dictionary pointer
	r                 push the return stack pointer
	allot             allocate space in the dictionary
(1)	[                 switch to command mode
	]                 switch to compile mode
	::                compile ':' into the dictionary

"

more "
For more information either consult the manual pages forth(1) and libforth(1)
or consult the following sources:

	https://github.com/howerj/libforth 
	http://work.anapnea.net/html/html/projects.html

And for a larger tutorial:

	https://github.com/howerj/libforth/blob/master/readme.md

For resources on Forth:

	https://en.wikipedia.org/wiki/Forth_%28programming_language%29
	http://www.ioccc.org/1992/buzzard.2.design
	https://rwmj.wordpress.com/2010/08/07/jonesforth-git-repository/

 -- end --
" cr
;

( ==================== Blocks ================================= )

( @todo process invalid blocks [anything greater or equal to 0xFFFF] )
( @todo only already created blocks can be loaded, this should be
  corrected so one is created if needed )
( @todo better error handling )

-1 variable scr-var 
false variable dirty ( has the buffer been modified? )
: scr ( -- x : last screen used ) scr-var @ ;
b/buf chars table block-buffer ( block buffer - enough to store one block )

: update ( mark block buffer as dirty, so it will be flushed if needed )
	true dirty ! ;
: empty-buffers ( discard any buffers )
	false dirty ! block-buffer b/buf chars erase ;
: flush dirty @ if block-buffer chars> scr bsave drop false dirty ! then ;

: list ( block-number -- : display a block )
	flush
	dup dup scr <> if
		block-buffer chars> swap bload  ( load buffer into block buffer )
		swap scr-var !
	else
		2drop 0
	then
	-1 = if exit then                 ( failed to load )
	block-buffer chars> b/buf type ;  ( print buffer )

: block ( u -- addr : load block 'u' and push address to block )
	dup scr <> if flush block-buffer chars> swap bload then
	-1 = if -1 else block-buffer then ;

: save-buffers flush ;

: list-thru ( x y -- : list blocks x through to y )
	1+ swap 
	key drop
	do " screen no: " i . cr i list cr more loop ;


( ==================== Blocks ================================= )


( ==================== Miscellaneous ========================== )


( @todo use check-within in various primitives like "array" )
: check-within ( x min max -- : abort if x is not within a range ) 
	within not if " limit exceeded" cr abort then ;

: enum ( x " ccc" -- x+1 : define a series of enumerations )
	dup constant 1+ ; ( better would be a :enum ;enum syntax )

: >upper ( c -- C : convert char to uppercase iff lower case )
	dup lowercase? if bl xor then ;

: >lower ( C -- c : convert char to lowercase iff upper case )
	dup uppercase? if bl xor then ;

: <=> ( x y -- z : spaceship operator! )
	2dup
	> if 2drop -1 exit then
	< ;

: compare ( c-addr1 u1 c-addr2 u2 -- n : compare two strings, assumes strings are NUL terminated )
	rot min
	0 do ( should be ?do )
		2dup
		i + c@ swap i + c@ 
		<=> dup if leave else drop then
	loop
	2drop ;

: license ( -- : print out license information )
" 
The MIT License (MIT)

Copyright (c) 2016 Richard James Howe

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the 'Software'),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

"
;

: welcome ( -- : print out a stupid welcome message which most interpreters seems insistent on)
	" FORTH: libforth successfully loaded." cr
	" Type 'help' and press return for a basic introduction." cr
	" Type 'license' and press return to see the license. (MIT license)." cr
	ok ;

0 [if]
 
: max-int [ -1 1 rshift ] literal ; 
: < 2dup <> if max-int - u> else 0 then ; 
: > < not ; 


( see https://groups.google.com/forum/#!topic/comp.lang.forth/rT-79frog1g )
 0 constant case immediate  ( init count of ofs )

 : of  ( #of -- orig #of+1 / x -- )
    1+    ( count ofs )
    >r    ( move off the stack in case the control-flow )
	  ( stack is the data stack. )
    postpone over  postpone = ( copy and test case value )
    postpone if    ( add orig to control flow stack )
    postpone drop  ( discards case value if = )
    r> ;           ( we can bring count back now )
 immediate

 : endof  ( orig1 #of -- orig2 #of )
    >r    ( move off the stack in case the control-flow )
	  ( stack is the data stack. )
    postpone else
    r> ;  ( we can bring count back now )
 immediate

 : endcase ( orig 1..orign #of -- )
    postpone drop  ( discard case value )
    0 ?do
      postpone then
    loop ;
 immediate

: .r
	" <" rdepth u. " >" space
	rdepth if
		rdepth 0 do i column tab i r @ + @ u. loop
	then
	cr ;

[then]

( clean up the environment )
:hide
 scr-var block-buffer
 write-string do-string ')' alignment-bits print-string 
 compile-instruction dictionary-start hidden? hidden-mask instruction-mask 
 max-core dolist x x! x@ write-exit 
 max-string-length xt-token error-no-word
 original-exit
 restart-address pnum
 TrueFalse >instruction print-header 
 print-name print-start print-previous print-immediate 
 print-instruction xt-instruction defined-word? print-defined
 `state 
 `source-id `sin `sidx `slen `start-address `fin `fout `stdin
 `stdout `stderr `argc `argv `debug `invalid `top `instruction
 `stack-size `start-time 
;hide 

( Heres an interesting piece of code from
  http://c2.com/cgi/wiki?ForthSimplicity

 : IMMEDIATE?	-1 = ;
 : NEXTWORD	BL WORD FIND ;
 : NUMBER,	NUMBER POSTPONE LITERAL ;
 : COMPILEWORD	DUP IF IMMEDIATE? IF EXECUTE ELSE COMPILE, THEN ELSE NUMBER, THEN ;
 : ]	BEGIN NEXTWORD COMPILEWORD AGAIN ;
 : [	R> R> 2DROP ; IMMEDIATE	\ Breaks out of compiler into interpret mode again 
)
