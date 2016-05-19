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
	libforth.md : for more information about this interpreter
	libforth.h  : for information about the C API
	libforth.3  : for limited information about the C API
	libforth.c  : for the interpreter itself

The interpreter and this code originally descend from a Forth interpreter
written in 1992 for the International obfuscated C Coding Competition 

See:
	http://www.ioccc.org/1992/buzzard.2.design
)

( ========================== Basic Word Set ================================== )
( We'll begin by defining very simple words we can use later )

: < u< ; ( @todo fix this )
: > u> ; ( @todo fix this )
: logical ( x -- bool ) not not ;
: hidden-mask 0x80 ( Mask for the hide bit in a words MISC field ) ;
: instruction-mask 0x1f ( Mask for the first code word in a words MISC field ) ;
: hidden? ( PWD -- PWD bool ) dup 1+ @ hidden-mask and logical ;
: push-location 2 ;       ( location of special "push" word )
: compile-instruction 1 ; ( compile code word, threaded code interpreter instruction )
: run-instruction 2 ;     ( run code word, threaded code interpreter instruction )
: [literal] push-location , , ; ( this word probably needs a better name )
: literal immediate [literal] ; 
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
: mod ( x u -- remainder ) 2dup / * - ;
: */ ( n1 n2 n3 -- n4 ) * / ; ( warning: this does not use a double cell for the multiply )
: char key drop key ;
: [char] immediate char [literal] ;
: postpone immediate find , ;
: unless immediate ( bool -- : like 'if' but execute clause if false ) ' 0= , postpone if ;
: endif immediate ( synonym for 'then' ) postpone then ;
: bl ( space character ) [char]  ; ( warning: white space is significant after [char] )
: space bl emit ;
: . pnum space ;
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
: # dup ( x -- x : debug print ) pnum cr ;
: compile, ' , , ;   ( A word that writes , into the dictionary )
: >mark ( -- ) here 0 , ; 
: end immediate (  A synonym for until ) postpone until ;
: bye ( -- : quit the interpreter ) 0 r ! ;
: pick ( xu ... x1 x0 u -- xu ... x1 x0 xu )
	stack-start depth + swap - 1- @ ;

: rdrop ( R: x -- )
	r>           ( get caller's return address )
	r>           ( get value to drop )
	drop         ( drop it like it's hot )
	>r           ( return return address )
;

: again immediate 
	( loop unconditionally in a begin-loop:
		begin ... again )
	' branch , here - , ; 

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
	64 
;
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
	source-id-reg @ ;

: 2drop ( x y -- ) drop drop ;
: hide  ( token -- hide-token ) 
	( This hides a word from being found by the interpreter )
	dup 
	if 
		dup @ hidden-mask or swap tuck ! 
	else 
		drop 0 
	then 
;

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

: xt-token ( previous-word-address -- xt-token )
	( Given the address of the PWD field of a word this
	function will return an execution token for the word )
	1+    ( MISC field )
	dup 
	@     ( Contents of MISC field )
	instruction-mask and  ( Mask off the instruction )
	( If the word is not an immediate word, execution token pointer ) 
	compile-instruction = +
;

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
	push-location , ( write push into new word )
	here 2+ ,       ( push a pointer to data field )
	['] exit ,      ( write in an exit to new word data field is after exit )
	command-mode    ( return to command mode )
;

: <build immediate
	( @note ' command-mode-create , *nearly* works )
	' :: ,                               ( Make the defining word compile a header )
	write-quote push-location , compile, ( Write in push to the creating word )
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
	run-instruction ,     ( write a run in )
;
hider write-quote 

( ========================== CREATE DOES> ==================================== )

: array    ( length --  : create a array )          create allot does> + ;
: table    ( length --  : create a table )          create allot does>   ;
: variable ( initial-value -- : create a variable ) create ,     does>   ;
: constant ( value -- : create a constant )         create ,     does> @ ;

( do...loop could be improved, perhaps by not using the return stack
so much )
: do immediate
	' swap ,      ( compile 'swap' to swap the limit and start )
	' >r ,        ( compile to push the limit onto the return stack )
	' >r ,        ( compile to push the start on the return stack )
	here          ( save this address so we can branch back to it )
;

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
	>r
;

: loop immediate 1 [literal] ' addi , here - , ;
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
: repeater  ( n0 X -- n0 ... nX )        1 do dup loop ;
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
	pwd @ xt-token 
	' branch ,
	here - 1+ ,
;

: recurse immediate
	( This function implements recursion, although this interpreter
	allows calling a word directly. If used incorrectly this will
	blow up the return stack.

	We can test "recurse" with this factorial function:
	  : factorial  dup 2 < if drop 1 exit then dup 1- recurse * ;)
	pwd @ xt-token , ;
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

4096 constant max-string-length 
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

( ==================== CASE statements ======================== )

( for a simple case statement: 
	see Volume 2, issue 3, page 48 of Forth Dimensions at 
	http://www.forth.org/fd/contents.html 
: case immediate
	' over , ' = , postpone if ;

\ Example of case usage:
: monitor \ [ KEY -- ]
	." START: "
	41 case ." ASSIGN"     then 
	44 case ." DISPLAY"    then
	46 case ." FILL"       then
	47 case ." GO"         then
	53 case ." SUBSTITUTE" then
	\  else ." INSERT"     then
	."  :END" cr
	drop
;

\ case with no default branch that hides case value
: case immediate ' >r , ;
: of= r> r> tuck >r >r = ; \ R: x of -- x of, S: y -- bool 
: of immediate  ' of= , postpone if ;
: endof immediate postpone then ;
: endcase immediate ' rdrop , ; 
)

: case immediate 
	' branch , 3 ,   ( branch over the next branch )
	here ' branch ,  ( mark: place endof branches back to with again )
	>mark swap ;     ( mark: place endcase writes jump to with then )

: over= over = ; 
: of      immediate ' over= , postpone if ;
: endof   immediate over postpone again postpone then ;
: endcase immediate postpone then drop ;

( 
: monitor \ [ KEY -- ]

	." START: " 
	case 
		." CASING "
	[char] A of ." ASSIGN "     endof 
	[char] D of ." DISPLAY "    endof
	[char] F of ." FILL "       endof
	[char] G of ." GO "         endof
	[char] S of ." SUBSTITUTE " endof
	           ." INSERT  " 
	endcase
	."  :END" cr
;

0 monitor 
char F monitor
char S monitor
char A monitor
char D monitor
char G monitor )

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
	again
;

: count ( c-addr1 -- c-addr2 u ) dup c@ swap 1+ swap ;

: fill ( c-addr u char -- )
	( fill in an area of memory with a character, only if u is greater than zero )
	-rot
	0 do 2dup i + c! loop
	2drop 
;

: spaces ( n -- : print n spaces ) 0 do space loop ;
: erase ( addr u : erase a block of memory )
	chars> swap chars> swap 0 fill ;

: blank ( c-addr u )
	( given a character address and length, this fills that range with spaces )
	bl fill ;

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
	then
;

: [else] ( discard input until [then] encounter, nesting for [if] )
	reset-nest
	begin
		find
		dup nest? unnest?
		end-nest?
	until
;
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

: lister 0 counter ! swap do i counted-column i @ . loop ;

( @todo dump should print out the data as characters as well, if printable )
: dump  ( addr u -- : dump out 'u' cells of memory starting from 'addr' )
	base @ >r hex 1+ over + lister r> base ! cr ;
:hide counted-column counter lister ;hide

: forgetter ( pwd-token -- : forget a found word and everything after it )
	dup @ pwd ! h ! ;

: forget ( WORD -- : forget word and every word defined after it )
	find 1- forgetter ;

: marker ( WORD -- : make word the forgets itself and words after it)
	:: pwd @ [literal] ' forgetter , postpone ; ;
hider forgetter

: ?dup-if immediate ( x -- x | - ) 
	' ?dup , postpone if ;

: type ( c-addr u -- : print out 'u' characters at c-addr )
	0 do dup c@ emit 1+ loop ;

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
	CSI pnum if ." ;1" then ." m" ;

: at-xy ( x y -- ) ( set ANSI terminal cursor position to x y ) CSI pnum ';' emit pnum ." H" ;
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

: .s    ( -- : print out the stack for debugging )
	depth if
		depth 0 do i column tab i pick . loop
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
	reset-column
	pwd @ ( Load most recently defined word )
	begin 
		dup 
		hidden? hide-words @ and
		not if 
			name
			print tab  
			auto-column
		else 
			drop 
		then 
		@  ( Get pointer to previous word )
		dup dictionary-start < ( stop if pwd no longer points to a word )
	until 
	drop cr 
; 
hider hide-words

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
	again ;

: TrueFalse if " true" else " false" then ;
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
	cr "  -- press any key to continue -- " key drop cr ;

: debug-help " debug mode commands 
	h - print help
	q - exit containing word
	r - print registers
	s - print stack
	c - continue on with execution 
" ;
: debug-prompt ." debug> " ;
: debug ( a work in progress, debugging support )
	cr
	begin
		debug-prompt
		key dup '\n' <> if source accept drop then
		case
			[char] h of debug-help endof
			[char] q of drop rdrop exit ( call abort when this exists ) endof
			[char] r of registers endof
			\ [char] d of dump endof \ implement read in number
			[char] s of >r .s r>  endof 
			[char] c of drop exit endof
		endcase 
	again ;
hider debug-prompt

0 variable cf
: code>pwd ( CODE -- PWD/0 )
	( given a pointer to a executable code field
	this words attempts to find the PWD field for
	that word, or return zero )
	dup here             >= if drop 0 exit then ( cannot be a word, too small )
	dup dictionary-start <= if drop 0 exit then ( cannot be a word, too large )
	cf !
	pwd @ dup @ ( p1 p2 )
	begin
		over ( p1 p2 p1 )
		cf @ <= swap cf @ > and if exit then
		dup 0=                  if exit then 
		dup @ swap 
	again
;
hider cf

: end-print ( x -- ) "  => " . " )" cr ;
: word-printer
	( attempt to print out a word given a words code field
	WARNING: This is a dirty hack at the moment
	NOTE: given a pointer to somewhere in a word it is possible
	to work out the PWD by looping through the dictionary to
	find the PWD below it )
	1- dup @ -1 =             if " ( noname )" end-print exit then
	   dup  " ( " code>pwd dup if name print else drop " data" then 
	        end-print ;
hider end-print

: decompile ( code-field-ptr -- )
	( @todo Rewrite, simplify and get this working correctly
 
	This word expects a pointer to the code field of a word, it
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
	exit     There are two definitions of exit, the one used in
		 ';' and the one everything else uses, this is used
		 to determine the actual end of the word
	literals Literals can be distinguished by their low value,
	         which cannot possibly be a word with a name, the
	         next field is the actual literal 

	Of special difficult is processing 'if' 'else' 'then' statements,
	this will require keeping track of '?branch'.

	Also of note, a number greater than "here" must be data )
	255 0
	do
		tab 
		dup @ push-location                      = if " ( literal => " 1+ dup @ . 1+ " )" cr tab then
		dup @ [ find branch   xt-token ] literal = if " ( branch => "  1+ dup @ . " )" cr dup dup @ dump dup @ + cr tab then
		dup @ [ original-exit xt-token ] literal = if " ( exit )"  i cr leave  then
		dup @ word-printer
		1+
	loop
	cr
	255 = if " decompile limit exceeded" cr then
	drop ;

hider word-printer

: xt-instruction ( extract instruction from execution token ) 
	xt-token @ >instruction ;
( these words expect a pointer to the PWD field of a word )
: defined-word?      xt-instruction run-instruction = ;
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
	0 source-id-reg ! ( @todo store stdin in input file )
	]
	restart-address r ! ;

: abort empty-stack quit ;

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
	save load         save or load a block at address to indexed file
	swap              swap first two values on the stack
	dup               duplicate the top of the stack
	drop              pop and drop a value
	over              copy the second stack value over the first
	.                 pop the top of the stack and print it
	print             print a NUL terminated string at a character address
	depth             get the current stack depth
	clock             get the time since execution start in milliseconds
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

	https://github.com/howerj/libforth/blob/master/libforth.md

For resources on Forth:

	https://en.wikipedia.org/wiki/Forth_%28programming_language%29
	http://www.ioccc.org/1992/buzzard.2.design
	https://rwmj.wordpress.com/2010/08/07/jonesforth-git-repository/

 -- end --
" cr
;

( clean up the environment )
:hide
 write-string do-string accept-string ')' alignment-bits print-string '"'
 compile-instruction dictionary-start hidden? hidden-mask instruction-mask 
 max-core push-location run-instruction stack-start x x! x@ write-exit 
 accepter max-string-length source-id-reg xt-token error-no-word
 original-exit
 restart-address pnum
 decompile TrueFalse >instruction print-header 
 print-name print-start print-previous print-immediate 
 print-instruction xt-instruction defined-word? print-defined

;hide

( ==================== Blocks ================================= )

( @todo process invalid blocks [anything greater or equal to 0xFFFF] )

-1 variable scr-var 
false variable dirty ( has the buffer been modified? )
: scr ( -- x : last screen used ) scr-var @ ;
b/buf chars table block-buffer ( block buffer - enough to store one block )

: update true dirty ! ;
: empty-buffers false dirty ! block-buffer b/buf chars erase ;
: flush dirty @ if block-buffer chars> scr bsave drop false dirty ! then ;

: list
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

hider scr-var
hider block-buffer

( ==================== Blocks ================================= )

( \ Test code
( 
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

0 variable x
0 variable y
4 variable scroll-speed
16 variable paint-speed
10 variable wait-time

: @x random 80 mod x ! x @ ;
: @y random 40 mod y ! y @ ;
: maybe-scroll random scroll-speed @ mod 0= if cr then ;
: star [char] * emit ;
: paint @x @y at-xy star ;
: maybe-paint random paint-speed @ mod 0= if paint then ;
: wait wait-time @ ms ;
: screen-saver
	page
	hide-cursor
	begin
		maybe-scroll
		maybe-paint
		wait
	again ;
)


( TODO
	* Evaluate 
	* By adding "c@" and "c!" to the interpreter I could remove print
	and put string functionality earlier in the file
	* Add unit testing to the end of this file
	* Word, Parse, other forth words
	* Add a dump core and load core to the interpreter?
	* add "j" if possible to get outer loop context
	* FORTH, VOCABULARY 
	* "Value", "To", "Is"
	* A small block editor
	* more help commands would be good, such as "help-ANSI-escape",
	"tutorial", etc.
	* Abort and Abort", this could be used to implement words such
	as "abort if in compile mode", or "abort if in command mode".
	* Todo Various different assemblers [assemble VM instructions,
	native instructions, and cross assemblers]
	* common words and actions should be factored out to simplify
	definitions of other words, their standards compliant version found
	if any
	* Various Forth style guides should be applied to this code to
	make it more understandable, the Forth code here is admittedly
	not very Forth like
	* An assembler mode would execute primitives only, this would
	require a change to the interpreter
	* throw and exception
	* here documents, string literals
	* A set of words for navigating around word definitions would be
	help debugging words, for example:
		compile-field code-field field-translate
	would take a pointer to a compile field for a word and translate
	that into the code field
	* [ifdef] [ifundef]
	* if strings were stored in word order instead of byte order
	then external tools could translate the dictionary by swapping
	the byte order of it
	* store strings as length + string instead of ASCII strings
	* proper booleans should be used throughout
	* escaped strings
	* ideally all of the functionality of main_forth would be
	moved into this file
	* a byte code only version of the interpreter could be made,
	experimenting with porting a portable version of the byte code
	should also be done
	* fill move and the like are technically not compliant as
	they do not test if the number is negative, however that would
	unnessarily limit the range of operation
)

