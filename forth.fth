#!./forth
( Welcome to libforth, A dialect of Forth. Like all versions
of Forth this version is  a little idiosyncratic, but how
the interpreter works is documented here and in various
other files.

This file contains most of the start up code, some basic
start up code is executed in the C file as well which makes
programming at least bearable.  Most of Forth is programmed in
itself, which may seem odd if your back ground in programming
comes from more traditional language [such as C], although
less so if you know already know lisp.

For more information about this interpreter and Forth see:

https://en.wikipedia.org/wiki/Forth_%28programming_language%29

And within this code base:

	readme.md  : for a manual for this interpreter
	libforth.h : for information about the C API
	libforth.c : for the interpreter itself
	unit.c     : unit tests for libforth.c
	unit.fth   : unit tests against this file
	main.c     : an example interpreter

The interpreter and this code originally descend from a Forth
interpreter written in 1992 for the International obfuscated
C Coding Competition.

See:
	http://www.ioccc.org/1992/buzzard.2.design

The manual for the interpreter should be read first before
looking into this code. It is important to understand the
execution model of Forth, especially the differences between
command and compile mode, and how immediate and compiling
words work.

Each of these sections is clearly labeled and they are
generally in dependency order.

Unfortunately the code in this file is not as portable as it
could be, it makes assumptions about the size of cells provided
by the virtual machine, which will take time to rectify. Some
of the constructs are subtly different from the DPANs Forth
specification, which is usually noted. Eventually this should
also be fixed.

A little note on how code is formatted, a line of Forth code
should not exceed 64 characters. All words stack effect, how
many variables on the stack it accepts and returns, should
be commented with the following types:

	char / c  A character
	c-addr    An address of a character
	addr      An address of a cell
	r-addr    A real address*
	u         A unsigned number
	n         A signed number
	c" xxx"   The word parses a word
	c" x"     The word parses a single character
	xt        An execution token
	bool      A boolean value
	ior       A file error return status [0 on no error]
	fileid    A file identifier.
	CODE      A number from a words code field
	PWD       A number from a words previous word field
	* A real address is one outside the range of the
	Forth address space.

In the comments Forth words should be written in uppercase. As
this is supposed to be a tutorial about Forth and describe the
internals of a Forth interpreter, be as verbose as possible.

The code in this file will need to be modified to abide by
these standards, but new code should follow it from the start.

Doxygen style tags for documenting bugs, todos and warnings
should be used within comments. This makes finding code that
needs working on much easier.)

( ==================== Basic Word Set ======================== )

( We'll begin by defining very simple words we can use later,
these a very basic words, that perform simple tasks, they
will not require much explanation.

Even though the words are simple, their stack comment and
a description for them will still be included so external
tools can process and automatically extract the document
string for a given work. )

: postpone ( c" xxx" -- : postpone execution of a word )
	immediate find , ;

: constant
	::           ( compile word header )
	here 1 - h ! ( decrement dictionary pointer )
	( change instruction to CONST )
	here @ instruction-mask invert and doconst or here !
	here 1 + h ! ( increment dictionary pointer )
	,            ( write in value )
	postpone [ ; ( back into command mode )

( These constants are defined as a space saving measure,
numbers in a definition usually take up two cells, one
for the instruction to push the next cell and the cell
itself, for the most frequently used numbers it is worth
defining them as constants, it is just as fast but saves
a lot of space )
-1 constant -1
 0 constant  0
 1 constant  1
 2 constant  2
 3 constant  3

0 constant false
1 constant true ( this is not standards compliant )

( Confusingly the word CR prints a line feed )
10 constant lf   ( line feed )
lf constant nl   ( new line - line feed on Unixen )
13 constant cret ( carriage return )

-1 -1 1 rshift invert and constant min-signed-integer
min-signed-integer invert constant max-signed-integer

( The following version number is used to mark whether core
files will be compatible with each other, The version number
gets stored in the core file and is used by the loader to
determine compatibility )
4 constant version ( version number for the interpreter )

( This constant defines the number of bits in an address )
cell size 8 * * constant address-unit-bits 

( Bit corresponding to the sign in a number )
-1 -1 1 rshift and invert constant sign-bit 

: 1+ ( x -- x : increment a number )
	1 + ;

: 1- ( x -- x : decrement a number )
	1 - ;

: chars ( c-addr -- addr : convert a c-addr to an addr )
	size / ;

: chars> ( addr -- c-addr: convert an addr to a c-addr )
	size * ;

: tab ( -- : print a tab character )
	9 emit ;

: 0=  ( n -- bool : is 'n' equal to zero? )
	0 = ;

: not ( n -- bool : is 'n' true? )
	0= ;

: <> ( n n -- bool : not equal )
	= 0= ;

: logical ( n -- bool : turn a value into a boolean )
	not not ;

: 2, ( n n -- : write two values into the dictionary )
	, , ;

: [literal] ( n -- : write a literal into the dictionary )
	dolit 2, ;

: literal ( n -- : immediately compile a literal )
	immediate [literal] ;

: sliteral immediate ( I: c-addr u --, Run: -- c-addr u )
	swap [literal] [literal] ;

( NB. We could throw if not found )
: ['] ( I: c" xxx", Run: -- xt  )
	immediate find [literal] ;

: >instruction ( CODE -- u : extract instruction CODE field )
	instruction-mask and ;

( IMMEDIATE-MASK pushes the mask for the compile bit,
which can be used on a CODE field of a word )
1 compile-bit lshift constant immediate-mask

: hidden? ( PWD -- PWD bool : is a word hidden? )
	dup 1+ @ hidden-mask and logical ;

: compiling? ( PWD -- PWD bool : is a word immediate? )
	dup 1+ @ immediate-mask and logical ;

: cr  ( -- : emit a newline character )
	nl emit ;

: < ( x1 x2 -- bool : signed less than comparison )
	- dup if max-signed-integer u> else logical then ;

: > ( x1 x2 -- bool : signed greater than comparison )
	- dup if max-signed-integer u< else logical then ;

( The pad area is an area used for temporary storage, some
words use it, although which ones do should be kept to a
minimum. It is an area of space #pad amount of characters
after the latest dictionary definition. The area between
the end of the dictionary and start of PAD space is used for
pictured numeric output [<#, #, #S, #>]. It is best not to
use the pad area that much. )
128 constant #pad

: pad ( -- addr : push pointer to the pad area )
	here #pad + ;

: 2literal immediate ( n n -- : compile two literals )
	swap [literal] [literal] ;

: latest ( get latest defined word )
	pwd @ ;

: stdin  ( -- fileid : push fileid for standard input )
	`stdin  @ ;

: stdout ( -- fileid : push fileid for standard output )
	`stdout @ ;

: stderr ( -- fileid : push fileid for the standard error )
	`stderr @ ;

: stdin? ( -- bool : are we reading from standard input )
	`fin @ stdin = ;

: *+ ( n1 n2 n3 -- n )
	* + ;
	
: 2- ( n -- n : decrement by two )
	2 - ;

: 2+ ( n -- n : increment by two )
	2 + ;

: 3+ ( n -- n : increment by three )
	3 + ;

: 2* ( n -- n : multiply by two )
	1 lshift ;

: 2/ ( n -- n : divide by two )
	1 rshift ;

: 4* ( n -- n : multiply by four )
	2 lshift ;

: 4/ ( n -- n : divide by four )
	2 rshift ;

: 8* ( n -- n : multiply by eight )
	3 lshift ;

: 8/ ( n -- n : divide by eight )
	3 rshift ;

: 256* ( n -- n : multiply by 256 )
	8 lshift ;

: 256/ ( n -- n : divide by 256 )
	8 rshift ;

: 2dup ( n1 n2 -- n1 n2 n1 n2 : duplicate two values )
	over over ;

: mod ( u1 u2 -- u : calculate the remainder of u1/u2 )
	2dup / * - ;

: um/mod ( u1 u2 -- rem quot : remainder and quotient of u1/u2 )
	2dup / >r mod r> ;

( NB. this does not use a double cell for the multiply )
: */ ( n1 n2 n3 -- n4 : [n2*n3]/n1 )
	 * / ;

: char ( -- n : read in a character from the input steam )
	key drop key ;

: [char] ( c" x" -- R: -- char : immediately compile next char )
	immediate char [literal] ;

( COMPOSE is a high level function that can take two executions
tokens and produce a unnamed function that can do what they
both do.

It can be used like so:

	:noname 2 ;
	:noname 3 + ;
	compose
	execute 
	.
	5 <-- 5 is printed!

I have not found a use for it yet, but it sure is neat.)

: compose ( xt1 xt2 -- xt3 : create a function from xt-tokens )
	>r >r            ( save execution tokens )
	postpone :noname ( create a new :noname word )
	r> ,             ( write first token )
	r> ,             ( write second token )
	(;) ;            ( terminate new :noname )

( CELLS is a word that is not needed in this interpreter but
it required to write portable code [I have probably missed
quite a few places where it should be used]. The reason it
is not needed in this interpreter is a cell address can only
be in multiples of cells, not in characters. This should be
addressed in the libforth interpreter as it adds complications
when having to convert to and from character and cell address.)

: cells ( n1 -- n2 : convert cell count to address count)
	immediate  ;

: cell+ ( a-addr1 -- a-addr2 )
	cell + ;

: cell- ( a-addr1 -- addr2 )
	cell - ;

: negative? ( x -- bool : is a number negative? )
	sign-bit and logical ;

: mask-byte ( x -- x : generate mask byte )
	8* 255 swap lshift ;

: select-byte ( u i -- c )
	8* rshift 0xff and ;

: xt-instruction ( PWD -- CODE : extract instruction from PWD )
	cell+ @ >instruction ;

: defined-word? ( PWD -- bool : is defined or a built-in word)
	xt-instruction dolist = ;

: char+ ( c-addr -- c-addr : increment a c-addr one c-addr )
	1+ ;

: 2chars ( c-addr1 c-addr2 -- addr addr : chars on two c-addr) 
	chars swap chars swap ;

: 2chars>  ( addr addr -- c-addr c-addr: chars> on two addr )
	chars> swap chars> swap ;

: hex     ( -- : print out hex )
	16 base ! ;

: octal   ( -- : print out octal )
	8 base ! ;

: binary  ( -- : print out binary )
	2 base ! ;

: decimal ( -- : print out decimal )
	0 base ! ;

: negate ( x -- x )
	-1 * ;

: abs ( x -- u : return the absolute value of a number )
	dup negative? if negate then ;

: square ( x -- x )
	dup * ;

: sum-of-squares ( a b -- c : compute a^2 + b^2 to get c )
	square swap square + ;

: drup   ( x y -- x x )
	drop dup ;

: +! ( x addr -- : add x to a value stored at addr )
	tuck @ + swap ! ;

: 1+! ( addr -- : increment a value at an address )
	1 swap +! ;

: 1-! ( addr -- : decrement a value at an address )
	-1 swap +! ;

: c+! ( x c-addr -- : add x to a value stored at c-addr )
	tuck c@ + swap c! ;

: toggle ( addr u -- : xor value at addr with u )
	over @ xor swap ! ;

: lsb ( x -- x : mask off the least significant byte of a cell )
	255 and ;

: \ ( -- : immediate word, used for single line comments )
	immediate begin key nl = until ;

: ?dup ( x -- ? )
	dup if dup then ;

: min ( n n -- n : return the minimum of two integers )
	2dup < if drop else nip then  ;

: max ( n n -- n : return the maximum of two integers )
	2dup > if drop else nip then  ;

: umin ( u u -- u : return the minimum of two unsigned numbers )
	2dup u< if drop else nip then ;

: umax ( u u -- u : return the maximum of two unsigned numbers )
	2dup u> if drop else nip then ;

: limit ( x min max -- x : limit x with a minimum and maximum )
	rot min max ;

: >= ( n n -- bool )
	< not ;

: <= ( n n -- bool )
	> not ;

: 2@ ( a-addr -- u1 u2 : load two consecutive memory cells )
	dup 1+ @ swap @ ;

: 2! ( u1 u2 a-addr -- : store two values at two consecutive memory cells )
	2dup ! nip 1+ ! ;

( @bug I don't think this is correct. )
: r@  (  -- u, R: u -- )
	r> r @ swap >r ;

: rp@ (  -- u, R: u -- )
	r> r @ swap >r ;

: 0> ( n -- bool )
	0 > ;

: 0<= ( n -- bool )
	0> not ;

: 0< ( n -- bool )
	0 < ;

: 0>= ( n -- bool )
	0< not ;

: 0<> ( n -- bool )
	0 <> ;

: signum ( n -- -1 | 0 | 1 : Signum function )
	dup 0> if drop  1 exit then
	    0< if      -1 exit then
	0 ;

: nand ( u u -- u : bitwise NAND )
	and invert ;

: odd ( u -- bool : is 'n' odd? )
	1 and ;

: even ( u -- bool : is 'n' even? )
	odd not ;

: nor  ( u u -- u : bitwise NOR  )
	or invert ;

: ms ( u -- : wait at least 'u' milliseconds )
	clock +  begin dup clock u< until drop ;

: sleep ( u -- : sleep for 'u' seconds )
	1000 * ms ;

: align ( addr -- addr : align an address, nop in this implemented )
	immediate  ;

: ) ( -- : do nothing, this allows easy commenting out of code )
	immediate ;

: bell ( -- : emit an ASCII BEL character )
	7 emit ;

: b/buf  ( -- u : bytes per buffer )
	1024 ;

: .d ( x -- x : debug print )
	dup . ;

: compile, ( x -- : )
	, ;

: >mark ( -- : write a hole into the dictionary and push a pointer to it )
	here 0 , ;

: <resolve ( addr -- : )
	here - , ;

: end immediate ( a synonym for until )
	postpone until ;

: bye ( -- : quit the interpreter )
	0 (bye) ;

: pick ( xu ... x1 x0 u -- xu ... x1 x0 xu )
	sp@ swap cells - cell - @ ;

: 3dup ( x1 x2 x3 -- x1 x2 x3 x1 x2 x3 : duplicate a set of three items )
	2 pick 2 pick 2 pick ;

: rpick ( R: xu ... x1 x0 u -- xu ... x1 x0 xu )
	r@ swap - @ ;

: within ( test low high -- bool : is test between low and high )
	over - >r - r> u< ;

: invalidate ( -- : invalidate this Forth core )
	1 `invalid ! ;

: signed ( x -- bool : return true if sign bit set )
	[ 1 size 8 * 1- lshift ] literal and logical ;

: u>=  ( x y -- bool : unsigned greater than or equal to )
	2dup u> >r = r> or ;

: u<=  ( x y -- bool : unsigned less than or equal to )
	u>= not ;

: rdrop ( R: x -- : drop a value from the return stack )
	r>           ( get caller's return address )
	r>           ( get value to drop )
	drop         ( drop it like it's hot )
	>r ;         ( return return address )

: rdup
	r>           ( get caller's return address )
	r>           ( get value to duplicate )
	dup          ( ... )
	>r >r >r ;   ( make it all work )

: chere ( -- c-addr : here as in character address units )
	here chars> ;

: source ( -- c-addr u )
	#tib   ( size of input buffer, in characters )
	tib ;  ( start of input buffer, in characters )

: stdin? ( -- bool : are we reading from standard in? )
	`fin @ `stdin @ = ;

: source-id ( -- 0 | -1 | file-id )
	( 	
	Value    Input Source
	-1       String
	0        Reading from user input / standard in
	file-id   )
	`source-id @
	0= if
		stdin? if 0 else `fin @ then
	else
		-1
	then ;

: under ( x1 x2 -- x1 x1 x2 )
	>r dup r> ;

: 2nip   ( n1 n2 n3 n4 -- n3 n4 )
	>r >r 2drop r> r> ;

: 2over ( n1 n2 n3 n4 – n1 n2 n3 n4 n1 n2 )
	>r >r 2dup r> swap >r swap r> r> -rot ;

: 2swap ( n1 n2 n3 n4 – n3 n4 n1 n2 )
	>r -rot r> -rot ;

: 2tuck ( n1 n2 n3 n4 – n3 n4 n1 n2 n3 n4 )
	2swap 2over ;

: 3drop ( n1 n2 n3 -- )
	drop 2drop ;

: 4drop  ( n1 n2 n3 n4 -- )
	2drop 2drop ;

: nos1+ ( x1 x2 -- x1+1 x2 : increment the next variable on that stack )
	swap 1+ swap ;

: ?dup-if immediate ( x -- x | - : ?dup and if rolled into one! )
	['] ?dup , postpone if ;

: ?if ( -- : non destructive if )
	immediate ['] dup , postpone if ;

: (hide) ( token -- hide-token : this hides a word from being found by the interpreter )
	?dup-if
		dup @ hidden-mask or swap tuck ! exit
	then 0 ;

: hide ( WORD -- : hide with drop )
	find dup if (hide) then drop ;

: reveal ( hide-token -- : reveal a hidden word )
	dup @ hidden-mask invert and swap ! ;

: ?exit ( x -- : exit current definition if not zero )
	if rdrop exit then ;

: decimal? ( c -- f : is character a number? )
	[char] 0 [ char 9 1+ ] literal within ;

: lowercase? ( c -- f : is character lower case? )
	[char] a [ char z 1+ ] literal within ;

: uppercase? ( C -- f : is character upper case? )
	[char] A [ char Z 1+ ] literal within ;

: alpha? ( C -- f : is character part of the alphabet? )
	dup lowercase? swap uppercase? or ;

: alphanumeric? ( C -- f : is character alphabetic or a number ? )
	dup alpha? swap decimal? or ;

: printable? ( c -- bool : is printable, excluding new lines and tables )
	32 127 within ;

: >upper ( c -- C : convert char to uppercase iff lower case )
	dup lowercase? if bl xor then ;

: >lower ( C -- c : convert char to lowercase iff upper case )
	dup uppercase? if bl xor then ;

: <=> ( x y -- z : spaceship operator! )
	2dup
	> if 2drop -1 exit then
	< ;

: start-address ( -- c-addr : push the start address  )
	`start-address @ ;

: >real-address ( c-addr -- r-addr : convert an interpreter address to a real address )
	start-address + ;

: real-address> ( c-addr -- r-addr : convert a real address to an interpreter address )
	start-address - ;

: peek ( r-addr -- n : )
	pad chars> >real-address swap size memory-copy pad @ ;

: poke ( n r-addr -- : )
	swap pad ! pad chars> >real-address size memory-copy ;

: die! ( x -- : controls actions when encountering certain errors )
	`error-handler ! ;

: start! ( cfa -- : set the word to execute at startup )
	`instruction ! ;	

: warm ( -- : restart the interpreter, warm restart )
	1 restart ;

: trip ( x -- x x x : triplicate a number )
	dup dup ;

: roll  ( xu xu-1 ... x0 u -- xu-1 ... x0 xu : move u+1 items on the top of the stack by u )
	[ smudge ] ( un-hide this word so we can call it recursively )
	dup 0 >
	if
		swap >r 1- roll r> swap
	else
		drop
	then ;
smudge

: 2rot (  n1 n2 n3 n4 n5 n6 – n3 n4 n5 n6 n1 n2 )
	5 roll 5 roll ;

: s>d ( x -- d : convert a signed value to a double width cell )
	( The if...else...then is only necessary as this Forths
	booleans are 0 and 1, not 0 and -1 as it usually is )
	dup 0< if -1 else 0 then  ;

: trace ( level -- : set tracing level )
	`debug ! ;

: verbose ( -- : get the log level )
	`debug @ ;

: count ( c-addr1 -- c-addr2 u : get a string whose first char is its length )
	dup c@ nos1+ ;

: bounds ( x y -- y+x x : make an upper and lower bound )
	over + swap ;

: aligned ( unaligned -- aligned : align a pointer )
	[ size 1- ] literal +
	[ size 1- ] literal invert and ;

: rdepth
	max-core `stack-size @ - r @ swap - ;

: argv ( -- r-addr : push pointer to array of string pointers to program )
	`argv @ ;

: argc ( -- u : push the number of arguments in the argv array )
	`argc @ ;

: +- ( x1 x2 -- x3 : copy the sign of x1 to x2 giving x3 )
	[ sign-bit 1- ] literal and
	swap
	sign-bit and or ;

: /string ( c-addr1 u1 u2 -- c-addr2 u2 : advance a string by n characters )
	over min rot over + -rot - ;

hide stdin?

: cfa immediate ( find-address -- cfa )
	( Given the address of the PWD field of a word this
	function will return an execution token for the word )
	 ;

: again immediate
	( loop unconditionally in a begin-loop:
		begin ... again )
	['] branch , <resolve ;



\ : >body ( xt -- a-addr : a-addr is data field of a CREATEd word )
\	cfa 5 + ;


: execute ( xt -- : given an execution token, execute the word )
	( create a word that pushes the address of a hole to write to.
	A literal takes up two words, '!' takes up one, that's right,
	some self modifying code! )
	[ here 3 cells + literal ] ( calculate place to write to )
	!                   ( write an execution token to a hole )
	[ 0 , ] ;           ( this is the hole we write )

: catch  ( xt -- exception# | 0 : return addr on stack )
	sp@ >r        ( xt : save data stack pointer )
	`handler @ >r ( xt : and previous handler )
	r@ `handler ! ( xt : set current handler )
	execute       (      execute returns if no throw )
	r> `handler ! (      restore previous handler )
	r> drop       (      discard saved stack ptr )
	0 ;           ( 0  : normal completion )

: throw  ( ??? exception# -- ??? exception# )
	?dup-if ( exc# \ 0 throw is no-op )
		`handler @ r ! ( exc# : restore prev return stack )
		r> `handler !  ( exc# : restore prev handler )
		r> swap >r     ( saved-sp : exc# on return stack )
		sp! drop r>    ( exc# : restore stack )
		( return to the caller of catch because return )
		( stack is restored to the state that existed )
		( when catch began execution )
	then ;

: interpret ( c1" xxx" ... cn" xxx" -- : This word implements the interpreter loop )
	begin
	['] read catch
	?dup-if [char] ! emit tab . cr then ( exception handler of last resort )
	again ;

: [interpret] ( c1" xxx" ... cn" xxx" -- : immediate version of interpret )
	immediate interpret ;

interpret ( use the new interpret word, which can catch exceptions )

find [interpret] cell+ start! ( the word executed on restart is now our new word )

( The following words are using in various control structure related
words to make sure they only execute in the correct state )

: ?comp ( -- : error if not compiling )
	state @ 0= if -14 throw then ;

: ?exec ( -- : error if not executing )
	state @    if -22 throw then ;

( begin...while...repeat These are defined in a very "Forth" way )
: while
	?comp
	immediate postpone if ( branch to repeats THEN ) ;

: whilst postpone while ;

: repeat immediate
	?comp
	swap            ( swap BEGIN here and WHILE here )
	postpone again  ( again jumps to begin )
	postpone then ; ( then writes to the hole made in if )

: never ( never...then : reserve space in word )
	immediate 0 [literal] postpone if ;

: unless ( bool -- : like IF but execute clause if false )
	immediate ?comp ['] 0= , postpone if ;

: endif ( synonym for THEN )
	immediate ?comp postpone then ;

( Experimental FOR ... NEXT )
: for immediate 
	?comp
	['] 1- ,
	['] >r ,
	here ;

: (next) ( -- bool,  R: val -- | val+1 )
	r> r> 1- dup 0< if drop >r 1 else >r >r 0 then ;

: next immediate
	?comp
	['] (next) , 
	['] ?branch , 
	here - , ;

( ==================== Basic Word Set ======================== )

( ==================== DOER/MAKE ============================= )
( DOER/MAKE is a word set that is quite powerful and is
described in Leo Brodie's book "Thinking Forth". It can be
used to make words whose behavior can change after they
are defined. It essentially makes the structured use of
self-modifying code possible, along with the more common
definitions of "defer/is".

According to "Thinking Forth", it has two purposes:

1. To change the state of a function.
2. To factor out common phrases of a words definition.

An example of the first instance:

	doer say
	: sad " Good bye, cruel World!" cr ;
	: happy " Hello, World!" cr ;

	: person say ;

	make person happy
	person \ prints "Good bye, cruel World!"

	make person sad
	person \ prints "Hello, World!"

An example of the second:

	doer op

	: sum \ n0 ... nX X -- sum<0..X>
		make op + 1 do op loop ;

	: mul \ n0 ... nX X -- mul<0..X>
		make op * 1 do op loop ;

The above example is a bit contrived, the definitions and
functionality are too simple for this to be worth factoring
out, but it shows how you can use DOER/MAKE. )

: noop ; ( -- : default word to execute for doer, does nothing )

: doer ( c" xxx" -- : make a work whose behavior can be changed by make )
	immediate ?exec :: ['] noop , (;) ;

: found? ( xt -- xt : thrown an exception if the xt is zero )
	dup 0= if -13 throw then ;

( It would be nice to provide a MAKE that worked with
execution tokens as well, although "defer" and "is" can be
used for that. MAKE expects two word names to be given as
arguments. It will then change the behavior of the first word
to use the second. MAKE is a state aware word. )

: make immediate ( c1" xxx" c2" xxx" : change parsed word c1 to execute c2 )
	find found? cell+
	find found?
	state @ if ( compiling )
		swap postpone 2literal ['] ! ,
	else ( command mode )
		swap !
	then ;

( ==================== DOER/MAKE ============================= )

( ==================== Extended Word Set ===================== )

: r.s ( -- : print the contents of the return stack )
	r>
	[char] < emit rdepth (.) drop [char] > emit
	space
	rdepth dup 0> if dup
	begin dup while r> -rot 1- repeat drop dup
	begin dup while rot dup . >r 1- repeat drop
	then  drop cr
	>r ;

: log ( u base -- u : command the _integer_ logarithm of u in base )
	>r
	dup 0= if -11 throw then ( logarithm of zero is an error )
	0 swap
	begin
		nos1+ rdup r> / dup 0= ( keep dividing until 'u' is zero )
	until
	drop 1- rdrop ;

: log2 ( u -- u : compute the _integer_ logarithm of u )
	2 log ;

: alignment-bits ( c-addr -- u : get the bits used for aligning a cell )
	[ 1 size log2 lshift 1- literal ] and ;

: time ( " ccc" -- n : time the number of milliseconds it takes to execute a word )
	clock >r
	find found? execute
	clock r> - ;

( defer...is is probably not standards compliant, it is still neat! )
: (do-defer) ( -- self : pushes the location into which it is compiled )
	r> dup >r 1- ;

: defer immediate ( " ccc" -- , Run Time -- location :
	creates a word that pushes a location to write an execution token into )
	?exec
	:: ['] (do-defer) , (;) ;

: is ( location " ccc" -- : make a deferred word execute a word )
	find found? swap ! ;

hide (do-defer)

( This RECURSE word is the standard Forth word for allowing
functions to be called recursively. A word is hidden from the
compiler until the matching ';' is hit. This allows for previous
definitions of words to be used when redefining new words, it
also means that words cannot be called recursively unless the
recurse word is used.

RECURSE calls the word being defined, which means that it
takes up space on the return stack. Words using recursion
should be careful not to take up too much space on the return
stack, and of course they should terminate after a finite
number of steps.

We can test "recurse" with this factorial function:

  : factorial dup 2 < if drop 1 exit then dup 1- recurse * ;

)
: recurse immediate
	?comp
	latest cell+ , ;

: myself ( -- : myself is a synonym for recurse )
	immediate postpone recurse ;

( The "tail" function implements tail calls, which is just a
jump to the beginning of the words definition, for example
this word will never overflow the stack and will print "1"
followed by a new line forever,

	: forever 1 . cr tail ;

Whereas

	: forever 1 . cr recurse ;

or

	: forever 1 . cr forever ;

Would overflow the return stack. )

hide tail
: tail ( -- : perform tail recursion in current word definition )
	immediate
	?comp
	latest cell+
	['] branch ,
	here - cell+ , ;

: factorial ( u -- u : factorial of u )
	dup 2 u< if drop 1 exit then dup 1- recurse * ;

: permutations ( u1 u2 -- u : number of ordered combinations )
	over swap - factorial swap factorial swap / ;

: combinations ( u1 u2 -- u : number of unordered combinations )
	dup dup permutations >r permutations r> /
	;

: average ( u1 u2 -- u : average of u1 and u2 ) 
	+ 2/ ;

: gcd ( u1 u2 -- u : greatest common divisor )
	dup if tuck mod tail then drop ;

: lcm ( u1 u2 -- u : lowest common multiple of u1 and u2 )
	2dup gcd / * ;

( From: https://en.wikipedia.org/wiki/Integer_square_root

This function computes the integer square root of a number.

This should be changed to the iterative algorithm so
it takes up less space on the stack - not that is takes up
a hideous amount as it is. )

: sqrt ( n -- u : integer square root )
	dup 0<  if -11 throw then ( does not work for signed values )
	dup 2 < if exit then      ( return 0 or 1 )
	dup ( u u )
	2 rshift recurse 2*       ( u sc : 'sc' == unsigned small candidate )
	dup                       ( u sc sc )
	1+ dup square             ( u sc lc lc^2 : 'lc' == unsigned large candidate )
	>r rot r> <               ( sc lc bool )
	if drop else nip then ;   ( return small or large candidate respectively )


( ==================== Extended Word Set ===================== )

( ==================== For Each Loop ========================= )
( The foreach word set allows the user to take action over an
entire array without setting up loops and checking bounds. It
behaves like the foreach loops that are popular in other
languages.

The foreach word accepts a string and an execution token,
for each character in the string it passes the current address
of the character that it should operate on.

The complexity of the foreach loop is due to the requirements
placed on it.  It cannot pollute the variable stack with
values it needs to operate on, and it cannot store values in
local variables as a foreach loop could be called from within
the execution token it was passed. It therefore has to store
all of it uses on the return stack for the duration of EXECUTE.

At the moment it only acts on strings as "/string" only
increments the address passed to the word foreach executes
to the next character address. )

\ (FOREACH) leaves the point at which the foreach loop
\ terminated on the stack, this allows programmer to determine
\ if the loop terminated early or not, and at which point.

: (foreach) ( c-addr u xt -- c-addr u : execute xt for each cell in c-addr u
	returning number of items not processed )
	begin
		3dup >r >r >r ( c-addr u xt R: c-addr u xt )
		nip           ( c-addr xt   R: c-addr u xt )
		execute       ( R: c-addr u xt )
		r> r> r>      ( c-addr u xt )
		-rot          ( xt c-addr u )
		1 /string     ( xt c-addr u )
		dup 0= if rot drop exit then ( End of string - drop and exit! )
		rot           ( c-addr u xt )
	again ;

\ If we do not care about returning early from the foreach
\ loop we can instead call FOREACH instead of (FOREACH),
\ it simply drops the results (FOREACH) placed on the stack.
: foreach ( c-addr u xt -- : execute xt for each cell in c-addr u )
	(foreach) 2drop ;

\ RETURN is used for an early exit from within the execution
\ token, it leaves the point at which it exited
: return ( -- n : return early from within a foreach loop function, returning number of items left )
	rdrop   ( pop off this words return value )
	rdrop   ( pop off the calling words return value )
	rdrop   ( pop off the xt token )
	r>      ( save u )
	r>      ( save c-addr )
	rdrop ; ( pop off the foreach return value )

\ SKIP is an example of a word that uses the foreach loop
\ mechanism. It  takes a string and a character and returns a
\ string that starts at  the first occurrence of that character
\ in that string - or until it  reaches the end of the string.

\ SKIP is defined in two steps, first there is a word,
\ (SKIP), that  exits the foreach loop if there is a match,
\ if there is no match it  does nothing. In either case it
\ leaves the character to skip until  on the stack.

: (skip) ( char c-addr -- char : exit out of foreach loop if there is a match )
	over swap c@ = if return then ;

\ SKIP then setups up the foreach loop, the char argument
\ will present on the stack when (SKIP) is executed, it must
\ leave a copy on the stack whatever happens, this is then
\ dropped at the end. (FOREACH) leaves the point at  which
\ the loop terminated on the stack, which is what we want.

: skip ( c-addr u char --  c-addr u : skip until char is found or until end of string )
	-rot ['] (skip) (foreach) rot drop ;
hide (skip)

( ==================== For Each Loop ========================= )

( ==================== Hiding Words ========================== )
( The two functions hide{ and }hide allow lists of words to
be hidden, instead of just hiding individual words. It stops
the dictionary from being polluted with meaningless words in
an easy way. )

: }hide ( should only be matched with 'hide{' )
	immediate -22 throw ;

: hide{ ( -- : hide a list of words, the list is terminated with "}hide" )
	?exec
	begin
		find ( find next word )
		dup [ find }hide ] literal = if
			drop exit ( terminate hide{ )
		then
		dup 0= if -15 throw then
		(hide) drop
	again ;

hide (hide)

( ==================== Hiding Words ========================== )

( The words described here on out get more complex and will
require more of an explanation as to how they work. )

( ==================== Create Does> ========================== )

( The following section defines a pair of words "create"
and "does>" which are a powerful set of words that can be
used to make words that can create other words. "create"
has both run time and compile time behavior, whilst "does>"
only works at compile time in conjunction with "create". These
two words can be used to add constants, variables and arrays
to the language, amongst other things.

A simple version of create is as follows

	: create :: dolist , here 2 cells + , ' exit , 0 state ! ;

But this version is much more limited.

"create"..."does>" is one of the constructs that makes Forth
Forth, it allows the creation of words which can define new
words in themselves, and thus allows us to extend the language
easily. )

: write-exit ( -- : A word that write exit into the dictionary )
	['] _exit , ;

: write-compile, ( -- : A word that writes , into the dictionary )
	['] , , ;

: create ( create a new work that pushes its data field )
	::               ( compile a word )
	dolit ,          ( write push into new word )
	here 2 cells + , ( push a pointer to data field )
	(;) ;            ( write exit, switch to command mode )

: <builds immediate
	( ['] create , *nearly* works )
	['] :: ,                           ( Make the defining word compile a header )
	dolit , dolit , write-compile,     ( Write in push to the creating word )
	['] here , ['] 3+ , write-compile, ( Write in the number we want the created word to push )
	dolit , >mark write-compile,       ( Write in a place holder 0 and push a pointer to to be used by does> )
	dolit , write-exit write-compile,  ( Write in an exit in the word we're compiling. )
	['] [ , ;            ( Make sure to change the state back to command mode )

\ : <builds immediate
\	['] create , 0 ,
\	;

: create immediate  ( create word is quite a complex forth word )
	state @
	if
		postpone <builds
	else
		create [ hide create ]
	then ;


: does> ( hole-to-patch -- )
	immediate
	?comp
	write-exit   ( we don't want the defining word to exit, but the *defined* word to )
	here swap !  ( patch in the code fields to point to )
	dolist , ;   ( write a run in )

hide{ write-compile, write-exit }hide

( Now that we have create...does> we can use it to create
arrays, variables and constants, as we mentioned before. )

: array ( u c" xxx" -- : create a named array of length u )
	create allot does> + ;

: variable ( x c" xxx" -- : create a variable will initial value of x )
	create ,     does>   ;

\ : constant ( x c" xxx" -- : create a constant with value of x )
\	create ,     does> @ ;

: table  ( u c" xxx" --, Run Time: -- addr u : create a named table )
	create dup , allot does> dup @ ;

: string ( u c" xxx" --, Run Time: -- c-addr u : create a named string )
	create dup , chars allot does> dup @ swap 1+ chars> swap ;

\ : +field  \ n <"name"> -- ; exec: addr -- 'addr
\    create over , +
\    does> @ + ;
\
\ : begin-structure  \ -- addr 0 ; -- size
\ 	create
\ 	here 0 0 ,  \ mark stack, lay dummy
\ 	does> @ ;   \ -- rec-len
\
\ : end-structure  \ addr n --
\    swap ! ;      \ set len
\
\ begin-structure point
\ 	point +field p.x
\ 	point +field p.y
\ end-structure
\
\ This should work...
\ : buffer: ( u c" xxx" --, Run Time: -- addr )
\	create allot ;

: 2constant
	create , , does> dup 1+ @ swap @ ;

: 2variable
	create , , does> ;

: enum ( x " ccc" -- x+1 : define a series of enumerations )
	dup constant 1+ ;

( ==================== Create Does> ========================== )

( ==================== Do ... Loop =========================== )

( The following section implements Forth's do...loop
constructs, the word definitions are quite complex as it
involves a lot of juggling of the return stack. Along with
begin...until do loops are one of the main looping constructs.

Unlike begin...until do accepts two values a limit and a
starting value, they are also words only to be used within a
word definition, some Forths extend the semantics so looping
constructs operate in command mode, this Forth does not do
that as it complicates things unnecessarily.

Example:

	: example-1
		10 1 do
			i . i 5 > if cr leave then loop
		100 . cr ;

	example-1

Prints:
	1 2 3 4 5 6

In "example-1" we can see the following:

1. A limit, 10, and a start value, 1, passed to "do".
2. A word called 'i', which is the current count of the loop.
3. If the count is greater than 5, we call a word call
LEAVE, this word exits the current loop context as well as
the current calling word.
4. "100 . cr" is never called. This should be changed in
future revision, but this version of leave exits the calling
word as well.

'i', 'j', and LEAVE *must* be used within a do...loop
construct.

In order to remedy point 4. loop should not use branch but
instead should use a value to return to which it pushes to
the return stack )

: (do)
	swap    ( swap the limit and start )
	r>      ( save our return stack to temporary variable )
	-rot    ( limit start return -- return start limit )
	>r      ( push limit onto return stack )
	>r      ( push start onto return stack )
	>r ;    ( restore our return address )

: do immediate  ( Run time: high low -- : begin do...loop construct )
	?comp
	['] (do) ,
	postpone begin ;

: (unloop)    ( -- , R: i limit -- : remove limit and i from  )
	r>           ( save our return address )
	rdrop        ( pop off i )
	rdrop        ( pop off limit )
	>r ;         ( restore our return stack )

: (+loop) ( x -- bool : increment loop variable by x and test it )
	r@ 1-        ( get the pointer to i )
	+!           ( add value to it )
	r@ 1- @      ( find i again )
	r@ 2- @      ( find the limit value )
	u>= ;

: (loop) ( -- bool : increment loop variable by 1 and test it )
	r@ 1-        ( get the pointer to i )
	1+!          ( add one to it )
	r@ 1- @      ( find the value again )
	r@ 2- @      ( find the limit value )
	u>= ;

: loop  ( -- : end do...loop construct )
	immediate ?comp ['] (loop) , postpone until ['] (unloop) , ;

: +loop ( x -- : end do...+loop loop construct )
	immediate ?comp ['] (+loop) , postpone until ['] (unloop) , ;

: leave ( -- , R: i limit return -- : break out of a do-loop construct )
	(unloop)
	rdrop ; ( return to the caller's caller routine )

: ?leave ( x -- , R: i limit return -- | i limit return : conditional leave )
	if
		(unloop)
		rdrop ( return to the caller's caller routine )
	then ;

: i ( -- i : Get current, or innermost, loop index in do...loop construct )
	r> r>   ( pop off return address and i )
	tuck    ( tuck i away )
	>r >r ; ( restore return stack )

: j ( -- j : Get outermost loop index in do...loop construct )
	4 rpick ;

( This is a simple test function for the looping, for interactive
testing and debugging:
 : mm 5 1 do i . cr 4 1 do j . tab i . cr loop loop ; )

: range ( nX nY -- nX nX+1 ... nY )
	nos1+ do i loop ;

: repeater ( n0 X -- n0 ... nX )
	1 do dup loop ;

: sum ( n0 ... nX X -- sum<0..X> )
	1 do + loop ;

: mul ( n0 ... nX X -- mul<0..X> )
	1 do * loop ;

: reverse ( x1 ... xn n -- xn ... x1 : reverse n items on the stack )
	0 do i roll loop ;

doer (banner)
make (banner) space

: banner ( n -- : )
	dup 0<= if drop exit then
	0 do (banner) loop ;

: zero ( -- : emit a single 0 character )
	[char] 0 emit ;

: spaces ( n -- : print n spaces if n is greater than zero )
	make (banner) space
	banner ;

: zeros ( n -- : print n spaces if n is greater than zero )
	make (banner) zero
	banner ;

hide{ (banner) banner }hide

: fill ( c-addr u char -- : fill in an area of memory with a character, only if u is greater than zero )
	-rot
	0 do 2dup i + c! loop
	2drop ;

: default ( addr u n -- : fill in an area of memory with a cell )
	-rot
	0 do 2dup i cells + ! loop
	2drop ;

: compare ( c-addr1 u1 c-addr2 u2 -- n : compare two strings, not quite compliant yet )
	>r swap r> min >r
	start-address + swap start-address + r>
	memory-compare ;

: erase ( addr u : erase a block of memory )
	2chars> 0 fill ;

: blank ( c-addr u : fills a string with spaces )
	bl fill ;

( move should check that u is not negative )
: move ( addr1 addr2 u -- : copy u words of memory from 'addr2' to 'addr1' )
	0 do
		2dup i + @ swap i + !
	loop
	2drop ;

( It would be nice if move and cmove could share more code, as they do exactly
  the same thing but with different load and store functions, cmove>  )
: cmove ( c-addr1 c-addr2 u -- : copy u characters of memory from 'c-addr2' to 'c-addr1' )
	0 do
		2dup i + c@ swap i + c!
	loop
	2drop ;



( ==================== Do ... Loop =========================== )

( ==================== String Substitution =================== )

: (subst) ( char1 char2 c-addr )
	3dup          ( char1 char2 c-addr char1 char2 c-addr )
	c@ = if ( char1 char2 c-addr char1 )
		swap c! ( match, substitute character )
	else    ( char1 char2 c-addr char1 )
		2drop ( no match )
	then ;

: subst ( c-addr u char1 char2 -- replace all char1 with char2 in string )
	swap
	2swap
	['] (subst) foreach 2drop ;
hide (subst)

0 variable c
0 variable sub
0 variable #sub

: (subst-all) ( c-addr : search in sub/#sub for a character to replace at c-addr )
	sub @ #sub @ bounds ( get limits )
	do
		dup ( duplicate supplied c-addr )
		c@ i c@ = if ( check if match )
			dup
			c chars c@ swap c! ( write out replacement char )
		then
	loop drop ;

: subst-all ( c-addr1 u c-addr2 u char -- replace chars in str1 if in str2 with char )
	c chars c! #sub ! sub ! ( store strings away )
	['] (subst-all) foreach ;

hide{ c sub #sub (subst-all) }hide

( ==================== String Substitution =================== )

0 variable column-counter
4 variable column-width

: column ( i -- )	
	column-width @ mod not if cr then ;

: column.reset		
	0 column-counter ! ;

: auto-column		
	column-counter dup @ column 1+! ;

0 variable x
: x! ( x -- )
	x ! ;

: x@ ( -- x )
	x @ ;

: 2>r ( x1 x2 -- R: x1 x2 )
	r> x! ( pop off this words return address )
		swap
		>r
		>r
	x@ >r ; ( restore return address )

: 2r> ( R: x1 x2 -- x1 x2 )
	r> x! ( pop off this words return address )
		r>
		r>
		swap
	x@ >r ; ( restore return address )

: 2r@ ( -- x1 x2 , R:  x1 x2 -- x1 x2 )
	r> x! ( pop off this words return address )
	r> r>
	2dup
	>r >r
	swap
	x@ >r ; ( restore return address )

: unused ( -- u : push the amount of core left )
	max-core here - ;

: accumulator  ( initial " ccc" -- : make a word that increments by a value and pushes the result )
	create , does> tuck +! @ ;

: counter ( n " ccc" --, Run Time: -- x : make a word that increments itself by one, starting from 'n' )
	create 1- , does> dup 1+! @ ;

0 variable delim
: accepter ( c-addr max delimiter -- i )
	( store a "max" number of chars at c-addr until "delimiter" encountered,
	the number of characters stored is returned )
	delim !  ( store delimiter used to stop string storage when encountered)
	0
	do
		key dup delim @ <>
		if
			over  c! 1+
		else ( terminate string )
			drop 0 swap c!
			i
			leave
		then
	loop
	-18 throw ; ( read in too many chars )
hide delim

: skip ( char -- : read input until string is reached )
	key drop >r 0 begin drop key dup rdup r> <> until rdrop ;

: word ( c -- c-addr : parse until 'c' is encountered, push transient counted string  )
	tib #tib 0 fill    ( zero terminal input buffer so we NUL terminate string )
	dup skip tib 1+ c!
	>r
	tib 2+
	#tib 1-
	r> accepter 1+
	tib c!
	tib ;
hide skip

: accept ( c-addr +n1 -- +n2 : see accepter definition )
	nl accepter ;

0xFFFF constant max-string-length

: (.")  ( char -- c-addr u )
	( Write a string into word being currently defined, this
	code has to jump over the string it has just put into the
	dictionary so normal execution of a word can continue. The
	length and character address of the string are left on the
	stack )
	>r              ( save delimiter )
	['] branch ,      ( write in jump, this will jump past the string )
	>mark           ( make hole )
	dup 1+ chars>   ( calculate address to write to )
	max-string-length
	r>              ( restore delimiter )
	accepter dup >r ( write string into dictionary, save index )
	aligned 2dup size / ( stack: length hole char-len hole )
	1+ dup allot    ( update dictionary pointer with string length )
	1+ swap !       ( write place to jump to )
	drop            ( do not need string length anymore )
	1+ chars>       ( calculate place to print )
	r> ;            ( restore index and address of string )

: length ( c-addr u -- u : push the length of an ASCIIZ string )
  tuck 0 do dup c@ 0= if 2drop i leave then 1+ loop ;

: asciiz? ( c-addr u -- : is a Forth string also a ASCIIZ string )
	tuck length <> ;

: asciiz ( c-addr u -- : trim a string until NUL terminator )
	2dup length nip ;

: (type) ( c-addr -- : emit a single character )
	c@ emit ;

: type ( c-addr u -- : print out 'u' characters at c-addr )
	['] (type) foreach ;
hide (type)

: do-string ( char -- : write a string into the dictionary reading it until char is encountered )
	(.")
	state @ if swap [literal] [literal] then ;

128 string sbuf
: s" ( "ccc<quote>" --, Run Time -- c-addr u )
	key drop sbuf 0 fill sbuf [char] " accepter sbuf drop swap ;
hide sbuf

: type,
	state @ if ['] type , else type then ;

: c"
	immediate key drop [char] " do-string ;

: "
	immediate key drop [char] " do-string type, ;

: sprint ( c -- : print out chars until 'c' is encountered )
	key drop ( drop next space )
	>r       ( save delimiter )
	begin
		key dup ( get next character )
		rdup r> ( get delimiter )
		<> if emit 0 then
	until rdrop ;

: .(
	immediate [char] ) sprint ;
hide sprint

: ."
	immediate key drop [char] " do-string type, ;

hide type,

( This word really should be removed along with any usages
of this word, it is not a very "Forth" like word, it accepts
a pointer to an ASCIIZ string and prints it out, it also does
not checking of the returned values from write-file )
: print ( c-addr -- : print out a string to the standard output )
	-1 over >r length r> swap stdout write-file 2drop ;

: ok
	" ok" cr ;

: empty-stack ( x-n ... x-0 -- : empty the variable stack )
	begin depth while drop repeat ;

: (quit) ( -- : do the work of quit, without the restart )
	0 `source-id !  ( set source to read from file )
	`stdin @ `fin ! ( read from stdin )
	postpone [      ( back into command mode )
	['] interpret start! ; ( set interpreter starting word )

: quit ( -- : Empty return stack, go back to command mode, read from stdin, interpret input )
	(quit)
	-1 restart  ;   ( restart the interpreter )

: abort
	-1 throw ;

: (abort") ( do the work of abort )
	(quit)
	-2 throw ;

: abort" immediate
	postpone "
	['] cr , ['] (abort") , ;


( ==================== Error Messages ======================== )
( This section implements a look up table for error messages,
the word MESSAGE was inspired by FIG-FORTH, words like ERROR
will be implemented later.

The DPANS standard defines a range of numbers which correspond
to specific errors, the word MESSAGE can decode these numbers
by looking up known words in a table.

See: http://lars.nocrew.org/dpans/dpans9.htm

       Code     Reserved for
       ----     ------------
        -1      ABORT
        -2      ABORT"
        -3      stack overflow
        -4      stack underflow
        -5      return stack overflow
        -6      return stack underflow
        -7      do-loops nested too deeply during execution
        -8      dictionary overflow
        -9      invalid memory address
        -10     division by zero
        -11     result out of range
        -12     argument type mismatch
        -13     undefined word
        -14     interpreting a compile-only word
        -15     invalid FORGET
        -16     attempt to use zero-length string as a name
        -17     pictured numeric output string overflow
        -18     parsed string overflow
        -19     definition name too long
        -20     write to a read-only location
        -21     unsupported operation [e.g., AT-XY on a
                too-dumb terminal]
        -22     control structure mismatch
        -23     address alignment exception
        -24     invalid numeric argument
        -25     return stack imbalance
        -26     loop parameters unavailable
        -27     invalid recursion
        -28     user interrupt
        -29     compiler nesting
        -30     obsolescent feature
        -31     >BODY used on non-CREATEd definition
        -32     invalid name argument [e.g., TO xxx]
        -33     block read exception
        -34     block write exception
        -35     invalid block number
        -36     invalid file position
        -37     file I/O exception
        -38     non-existent file
        -39     unexpected end of file
        -40     invalid BASE for floating point conversion
        -41     loss of precision
        -42     floating-point divide by zero
        -43     floating-point result out of range
        -44     floating-point stack overflow
        -45     floating-point stack underflow
        -46     floating-point invalid argument
        -47     compilation word list deleted
        -48     invalid POSTPONE
        -49     search-order overflow
        -50     search-order underflow
        -51     compilation word list changed
        -52     control-flow stack overflow
        -53     exception stack overflow
        -54     floating-point underflow
        -55     floating-point unidentified fault
        -56     QUIT
        -57     exception in sending or receiving a character
        -58     [IF], [ELSE], or [THEN] exception )

( The word X" compiles a counted string into a word definition, it
is useful as a space saving measure and simplifies our lookup table
definition. Instead of having to store a C-ADDR and it's length, we
only have to store a C-ADDR in the lookup table, which occupies only
one element instead of two. The strings used for error messages are
short, so the limit of 256 characters that counted strings present
is not a problem. )

: x" immediate ( c" xxx" -- c-addr : compile a counted string )
	[char] " word        ( read in a counted string )
	count -1 /string dup ( go back to start of string )
	postpone never >r    ( make a hole in the dictionary )
	chere >r             ( character marker )
	aligned chars allot  ( allocate space in hole )
	r> dup >r -rot cmove ( copy string from word buffer )
	r>                   ( restore pointer to counted string )
	r> postpone then ;   ( finish hole )

create lookup 64 cells allot ( our lookup table )

: nomsg ( -- c-addr : push counted string to undefined error message )
	x" Undefined Error" literal ;

lookup 64 cells find nomsg default

1 variable warning

: message ( n -- : print an error message )
	warning @ 0= if . exit then
	dup -63 1 within if abs lookup + @ count type exit then
	drop nomsg count type ;

1 counter #msg

: nmsg ( n -- : populate next slot in available in LOOKUP )
	lookup #msg cells + ! ;

x" ABORT"  nmsg
x" ABORT Double Quote"  nmsg
x" stack overflow"  nmsg
x" stack underflow"  nmsg
x" return stack overflow"  nmsg
x" return stack underflow"  nmsg
x" do-loops nested too deeply during execution"  nmsg
x" dictionary overflow"  nmsg
x" invalid memory address"  nmsg
x" division by zero"  nmsg
x" result out of range"  nmsg
x" argument type mismatch"  nmsg
x" undefined word"  nmsg
x" interpreting a compile-only word"  nmsg
x" invalid FORGET"  nmsg
x" attempt to use zero-length string as a name"  nmsg
x" pictured numeric output string overflow"  nmsg
x" parsed string overflow"  nmsg
x" definition name too long"  nmsg
x" write to a read-only location"  nmsg
x" unsupported operation "  nmsg
x" control structure mismatch"  nmsg
x" address alignment exception"  nmsg
x" invalid numeric argument"  nmsg
x" return stack imbalance"  nmsg
x" loop parameters unavailable"  nmsg
x" invalid recursion"  nmsg
x" user interrupt"  nmsg
x" compiler nesting"  nmsg
x" obsolescent feature"  nmsg
x" >BODY used on non-CREATEd definition"  nmsg
x" invalid name argument "  nmsg
x" block read exception"  nmsg
x" block write exception"  nmsg
x" invalid block number"  nmsg
x" invalid file position"  nmsg
x" file I/O exception"  nmsg
x" non-existent file"  nmsg
x" unexpected end of file"  nmsg
x" invalid BASE for floating point conversion"  nmsg
x" loss of precision"  nmsg
x" floating-point divide by zero"  nmsg
x" floating-point result out of range"  nmsg
x" floating-point stack overflow"  nmsg
x" floating-point stack underflow"  nmsg
x" floating-point invalid argument"  nmsg
x" compilation word list deleted"  nmsg
x" invalid POSTPONE"  nmsg
x" search-order overflow"  nmsg
x" search-order underflow"  nmsg
x" compilation word list changed"  nmsg
x" control-flow stack overflow"  nmsg
x" exception stack overflow"  nmsg
x" floating-point underflow"  nmsg
x" floating-point unidentified fault"  nmsg
x" QUIT"  nmsg
x" exception in sending or receiving a character"  nmsg
x" [IF], [ELSE], or [THEN] exception" nmsg

hide{ nmsg #msg nomsg }hide
 

( ==================== Error Messages ======================== )


( ==================== CASE statements ======================= )
( This simple set of words adds case statements to the
interpreter, the implementation is not particularly efficient,
but it works and is simple.

Below is an example of how to use the CASE statement,
the following word, "example" will read in a character and
switch to different statements depending on the character
input. There are two cases, when 'a' and 'b' are input, and
a default case which occurs when none of the statements match:

	: example
		char
		case
			[char] a of " a was selected " cr endof
			[char] b of " b was selected " cr endof

			dup \ encase will drop the selector
			" unknown char: " emit cr
		endcase ;

	example a \ prints "a was selected"
	example b \ prints "b was selected"
	example c \ prints "unknown char: c"

Other examples of how to use case statements can be found
throughout the code.

For a simpler case statement see, Volume 2, issue 3, page 48
of Forth Dimensions at http://www.forth.org/fd/contents.html )

: case immediate
	?comp
	['] branch , 3 cells , ( branch over the next branch )
	here ['] branch ,      ( mark: place endof branches back to with again )
	>mark swap ;         ( mark: place endcase writes jump to with then )

: over= ( x y -- [x 0] | 1 : )
	over = if drop 1 else 0 then ;

: of
	immediate ?comp ['] over= , postpone if ;

: endof
	immediate ?comp over postpone again postpone then ;

: endcase
	immediate ?comp ['] drop , 1+ postpone then drop ;

( ==================== CASE statements ======================= )

( ==================== Conditional Compilation =============== )
( The words "[if]", "[else]" and "[then]" implement conditional
compilation, they can be nested as well

See http://lars.nocrew.org/dpans/dpans15.htm for more
information

A much simpler conditional compilation method is the following
single word definition:

 : compile-line? 0= if [ find \\ , ] then ;

Which will skip a line if a conditional is false, and compile
it if true )

( These words really, really need refactoring, I could use the newly defined
  "defer" to help out with this )
0 variable      nest        ( level of [if] nesting )
0 variable      [if]-word   ( populated later with "find [if]" )
0 variable      [else]-word ( populated later with "find [else]")
: [then]        immediate ;
: reset-nest    1 nest ! ;
: unnest?       [ find [then] ] literal = if nest 1-! then ;
: nest?         [if]-word             @ = if nest 1+! then ;
: end-nest?     nest @ 0= ;
: match-[else]? [else]-word @ = nest @ 1 = and ;

: [if] ( bool -- : conditional execution )
	?exec
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
	?exec
	reset-nest
	begin
		find
		dup nest? unnest?
		end-nest?
	until ;

find [if] [if]-word !
find [else] [else]-word !

: ?( if postpone ( then ; \ conditionally read until ')'
: ?\ if postpone \ then ;
: 16bit\ size 2 <> if postpone \ then ;
: 32bit\ size 4 <> if postpone \ then ;
: 64bit\ size 8 <> if postpone \ then ;

hide{
	[if]-word [else]-word nest
	reset-nest unnest? match-[else]?
	end-nest? nest?
}hide

( ==================== Conditional Compilation =============== )

( ==================== Endian Words ========================== )
( This words are allow the user to determinate the endianess
of the machine that is currently being used to execute libforth,
they make heavy use of conditional compilation )

0 variable x

size 2 = [if] 0x0123           x ! [then]
size 4 = [if] 0x01234567       x ! [then]
size 8 = [if] 0x01234567abcdef x ! [then]

x chars> c@ 0x01 = constant endian

hide{ x }hide

: swap16 ( x -- x : swap the byte order of a 16 bit number )
	dup 256* 0xff00 and >r 256/ lsb r> or ;

size 4 >= [if]
	: swap32
		dup       0xffff and swap16 16 lshift swap
		16 rshift 0xffff and swap16 or ;
[then]

size 8 >= [if]
	: swap64 ( x -- x : swap the byte order of a 64 bit number )
		    dup       0xffffffff and swap32 32 lshift swap
		    32 rshift 0xffffffff and swap32 or ;
[then]

size 2 = [if]
	endian
	[if]      ( host is big endian )
	: >little ( x -- x : host byte order to little endian order )
		swap16 ;
	: >big    ( x -- x : host byte order to big endian order )
		;
	[else]    ( host is little endian )
	: >little ( x -- x : host byte order to little endian order )
		;
	: >big    ( x -- x : host byte order to big endian order )
		swap16 ;
	[then]
[then]

size 4 = [if]
	endian
	[if]      ( host is big endian )
	: >little ( x -- x : host byte order to little endian order )
		swap32 ;
	: >big    ( x -- x : host byte order to big endian order )
		;
	[else]    ( host is little endian )
	: >little ( x -- x : host byte order to little endian order )
		;
	: >big    ( x -- x : host byte order to big endian order )
		swap32 ;
	[then]
[then]

size 8 = [if]
	endian
	[if]      ( host is big endian )
	: >little ( x -- x : host byte order to little endian order )
		swap64 ;
	: >big    ( x -- x : host byte order to big endian order )
		;
	[else]    ( host is little endian )
	: >little ( x -- x : host byte order to little endian order )
		;
	: >big    ( x -- x : host byte order to big endian order )
		swap64 ;
	[then]
[then]

( ==================== Endian Words ========================== )

( ==================== Misc words ============================ )

: (base) ( -- base : unmess up libforth's base variable )
	base @ dup 0= if drop 10 then ;

: #digits ( u -- u : number of characters needed to represent 'u' in current base )
	dup 0= if 1+ exit then
	(base) log 1+ ;

: digits ( -- u : number of characters needed to represent largest unsigned number in current base )
	-1 #digits ;

: cdigits ( -- u : number of characters needed to represent largest characters in current base )
	0xff #digits ;

: .r ( u -- print a number taking up a fixed amount of space on the screen )
	dup #digits digits swap - spaces . ;

: ?.r ( u -- : print out address, right aligned )
	@ .r ;

0 variable counter

: counted-column ( index -- : special column printing for dump )
	counter @ column-width @ mod
	not if cr .r " :" space else drop then
	counter 1+! ;

: .chars ( x n -- : print a cell out as characters, upto n chars )
	0 ( from zero to the size of a cell )
	do
		dup                     ( copy variable to print out )
		size i 1+ - select-byte ( select correct byte )
		dup printable? not      ( is it not printable )
		if drop [char] . then   ( print a '.' if it is not )
		emit                    ( otherwise print it out )
	loop
	drop ; ( drop cell we have printed out )

: lister ( addr u addr -- )
	0 counter ! 1- swap
	do
		dup counted-column 1+ i ?.r i @ size .chars space
	loop ;

: dump  ( addr u -- : dump out 'u' cells of memory starting from 'addr' )
	1+ over + under lister drop
	cr ;

hide{ counted-column counter }hide

( Fence can be used to prevent any word defined before it from being forgotten
Usage:
	here fence ! )
0 variable fence

: ?fence ( addr -- : throw an exception of address is before fence )
	fence @ u< if -15 throw then ;

: (forget) ( pwd-token -- : forget a found word and everything after it )
	dup 0= if -15 throw then         ( word not found! )
	dup ?fence
	dup @ pwd ! h ! ;

: forget ( c" xxx" -- : forget word and every word defined after it )
	find 1- (forget) ;

0 variable fp ( FIND Pointer )

: rendezvous ( -- : set up a rendezvous point )
	here ?fence
	here fence !
	latest fp ! ;

: retreat ( -- : retreat to the rendezvous point, forgetting any words )
	fence @ h !
	fp @ pwd ! ;

hide{ fp }hide

: marker ( c" xxx" -- : make word the forgets itself and words after it)
	:: latest [literal] ['] (forget) , (;) ;
here fence ! ( This should also be done at the end of the file )
hide (forget)

: ** ( b e -- x : exponent, raise 'b' to the power of 'e')
	?dup-if
		over swap
		1 do over * loop
		nip
	else
		drop 1
	then ;

0 variable a
0 variable b
0 variable m
: equal ( a1...an b1...bn n -- a1...an b1...bn bool : determine if two lists are equal )
	( example:
		1 2 3
		1 2 3
		3 equal
	returns: 1 )
	dup m ! 1+ 1  ( store copy of length and use as loop index )
	do
		i 1-       pick b ! ( store ith element of list in b1...bn )
		i m @ + 1- pick a ! ( store ith element of list in a1...an )
		a @ b @ <>          ( compare a and b for equality )
		if 0 leave then     ( unequal, finish early )
	loop 1 ; ( lists must be equal )

hide{ a b m }hide

: ndrop ( u1...un n -- : drop n items )
	?dup-if 0 do drop loop then ;

: caesar ( c key -- o : encode a alphabetic character with a key using a generalization of the Caesar cipher )
	>r
	dup uppercase? if [char] A - r> + 26 mod [char] A + exit then
	dup lowercase? if [char] a - r> + 26 mod [char] a + exit then
	rdrop ; ( it goes without saying that this should not be used for anything serious! )

: caesar-type ( c-addr u key : type out encoded text with a Caesar cipher )
	-rot bounds do i c@ over caesar emit loop drop ;

: rot13 ( c -- c : encode a character with ROT-13 )
	13 caesar ;

: rot13-type ( c-addr u : print string in ROT-13 encoded form )
	13 caesar-type ;

\ s" abcdefghijklmnopqrstuvwxyz" rot13-type -> nopqrstuvwxyzabcdefghijklm
\ s" hello" rot13-type -> uryyb

( ==================== Misc words ============================ )

( ==================== Pictured Numeric Output =============== )
( Pictured numeric output is what Forths use to display
numbers to the screen, this Forth has number output methods
built into the Forth kernel and mostly uses them instead,
but the mechanism is still useful so it has been added.

NB. Pictured number output should act on a double cell
number not a single cell number )

0 variable hld

: overflow ( -- : check if we overflow the hold area )
 	here chars> pad chars> hld @ - u> if -17 throw then ;

: hold ( char -- : add a character to the numeric output string )
	overflow pad chars> hld @ - c! hld 1+! ;

: holds ( c-addr u -- : hold an entire string )
  begin dup while 1- 2dup + c@ hold repeat 2drop ;

: <# ( -- : setup pictured numeric output )
	0 hld ! ;

: sign ( -- : add a sign to the pictured numeric output string )
	[char] - hold ;

: # ( x -- x : divide x by base, turn into a character, put in pictured output string )
	(base) um/mod swap
  	dup 9 u>
  	if 7 + then
  	48 + hold ;

: #s ( x -- 0 : repeatedly call # on x until x is zero )
	begin # dup 0= until ;

: #> ( -- c-addr u : end pictured output conversion, push output string to stack )
	0 hold   ( NUL terminate string, just in case )
	hld 1-!  ( but do not include that in the count )
	pad chars> hld @
	tuck - 1+ swap ;

doer characters
make characters spaces

: u.rc
	>r <# #s #> rot drop r> over - characters type ;

: u.r ( u n -- print a number taking up a fixed amount of space on the screen )
	make characters spaces u.rc ;

: u.rz ( u n -- print a number taking up a fixed amount of space on the screen, using leading zeros )
	make characters zeros u.rc ;

: u. ( u -- : display an unsigned number in current base )
	0 u.r ;

hide{ overflow u.rc characters }hide

( ==================== Pictured Numeric Output =============== )

( ==================== Numeric Input ========================= )
( The Forth executable can handle numeric input and does not
need the routines defined here, however the user might want
to write routines that use >NUMBER. >NUMBER is a generic word,
but it is a bit difficult to use on its own. )

: map ( char -- n|-1 : convert character in 0-9 a-z range to number )
	dup lowercase? if [char] a - 10 + exit then
	dup decimal?   if [char] 0 -      exit then
	drop -1 ;

: number? ( char -- bool : is a character a number in the current base )
	>lower map (base) u< ;

: >number ( n c-addr u -- n c-addr u : convert string )
	begin
		( get next character )
		2dup >r >r drop c@ dup number? ( n char bool, R: c-addr u )
		if   ( n char )
			swap (base) * swap map + ( accumulate number )
		else ( n char )
			drop
			r> r> ( restore string )
			exit
		then
		r> r> ( restore string )
		1 /string dup 0= ( advance string and test for end )
	until ;

hide{ map }hide

( ==================== Numeric Input ========================= )

( ==================== ANSI Escape Codes ===================== )
( Terminal colorization module, via ANSI Escape Codes

see: https://en.wikipedia.org/wiki/ANSI_escape_code
These codes will provide a relatively portable means of
manipulating a terminal )

27 constant 'escape'
: CSI 'escape' emit ." [" ;
0 constant black
1 constant red
2 constant green
3 constant yellow
4 constant blue
5 constant magenta
6 constant cyan
7 constant white
: foreground 30 + ;
: background 40 + ;
0 constant dark
1 constant bright
false variable colorize

: 10u. ( n -- : print a number in decimal )
	base @ >r decimal u. r> base ! ;

: color ( brightness color-code -- : set the terminal color )
	( set color on an ANSI compliant terminal,
	for example:
		bright red foreground color
	sets the foreground text to bright red )
	colorize @ 0= if 2drop exit then
	CSI 10u. if ." ;1" then ." m" ;

: at-xy ( x y -- : set ANSI terminal cursor position to x y )
	CSI 10u. [char] ; emit 10u. ." H" ;

: page  ( -- : clear ANSI terminal screen and move cursor to beginning )
	CSI ." 2J" 1 1 at-xy ;

: hide-cursor ( -- : hide the cursor from view )
	CSI ." ?25l" ;

: show-cursor ( -- : show the cursor )
	CSI ." ?25h" ;

: save-cursor ( -- : save cursor position )
	CSI ." s" ;

: restore-cursor ( -- : restore saved cursor position )
	CSI ." u" ;

: reset-color ( -- : reset terminal color to its default value)
	colorize @ 0= if exit then
	CSI ." 0m" ;

hide{ CSI 10u. }hide
( ==================== ANSI Escape Codes ===================== )

( ==================== Unit test framework =================== )

256 string estring    ( string to test )
0 variable #estring   ( actual string length )
0 variable start      ( starting depth )
0 variable result     ( result depth )
0 variable check      ( only check depth if -> is called )
0 variable dictionary ( dictionary pointer on entering { )
0 variable previous   ( PWD register on entering { )

: T ; ( hack until T{ can process words )

: -> ( -- : save depth in variable )
	1 check ! depth result ! ;

: test estring drop #estring @ ;

: fail ( -- : invalidate the forth interpreter and exit )
	invalidate bye ;

: neutral ( -- : neutral color )
	;

: bad ( -- : bad color )
	dark red foreground color ;

: good ( -- : good color )
	dark green foreground color ;

: die bad test type reset-color cr fail ;

: evaluate? ( bool -- : test if evaluation has failed )
	if ." evaluation failed" cr fail then ;

: failed bad ." failed" reset-color cr ;

: adjust ( x -- x : adjust a depth to take into account starting depth )
	start @ - ;

: no-check? ( -- bool : if true we need to check the depth )
	check @ 0= ;

: depth? ( -- : check if depth is correct )
	no-check? if exit then
	depth adjust          ( get depth and adjust for starting depth )
	result @ adjust 2* =  ( get results depth, same adjustment, should be
	                        half size of the depth )
	if exit then ( pass )
	failed
	." Unequal depths:" cr
	." depth:  " depth . cr
	." result: " result @ . cr
	die ;

: equal? ( -- : determine if results equals expected )
	no-check? if exit then
	result @ adjust equal
	if exit then
	failed
	." Result is not equal to expected values. " cr
	." Stack: " cr .s cr
	die ;

: display ( c-addr u -- : print out testing message in estring )
	verbose if neutral type else 2drop then ;

: pass ( -- : print out passing message )
	verbose if good ." ok " cr reset-color then ;

: save  ( -- : save current dictionary )
	pwd @ previous !
	here dictionary ! ;

: restore ( -- : restore dictionary )
	previous @ pwd !
	dictionary @ h ! ;

: T{  ( -- : perform a unit test )
	depth start !  ( save start of stack depth )
	0 result !     ( reset result variable )
	0 check  !     ( reset check variable )
	estring 0 fill ( zero input string )
	save           ( save dictionary state )
	key drop       ( drop next character, which is a space )
	estring [char] } accepter #estring ! ( read in string to test )
	test display   ( print which string we are testing )
	test evaluate  ( perform test )
	evaluate?      ( evaluate successfully? )
	depth?         ( correct depth )
	equal?         ( results equal to expected values? )
	pass           ( print pass message )
	restore        ( restore dictionary to previous state )
	no-check? if exit then
	result @ adjust 2* ndrop ( remove items on stack generated by test )
	;      

T{ }T 
T{ -> }T
T{ 1 -> 1 }T
T{ 1 2 -> 1 2 }T
T{ : c 1 2 ; c -> 1 2 }T

( @bug ';' smudges the previous word, but :noname does
not. )
T{ :noname 2 ; :noname 3 + ; compose execute -> 5 }T

hide{
	pass test display
	adjust start save restore dictionary previous
	evaluate? equal? depth? estring #estring result
	check no-check? die neutral bad good failed
}hide

( ==================== Unit test framework =================== )


( ==================== Pseudo Random Numbers ================= )
( This section implements a Pseudo Random Number generator, it
uses the xor-shift algorithm to do so.
See:
https://en.wikipedia.org/wiki/Xorshift
http://excamera.com/sphinx/article-xorshift.html
http://xoroshiro.di.unimi.it/
http://www.arklyffe.com/main/2010/08/29/xorshift-pseudorandom-number-generator/

The constants used have been collected from various places
on the web and are specific to the size of a cell. )

size 2 = [if] 13 constant a 9  constant b 7  constant c [then]
size 4 = [if] 13 constant a 17 constant b 5  constant c [then]
size 8 = [if] 12 constant a 25 constant b 27 constant c [then]

7 variable seed ( must not be zero )

: seed! ( x -- : set the value of the PRNG seed )
	dup 0= if drop 7 ( zero not allowed ) then seed ! ;

: random ( -- x : assumes word size is 32 bit )
	seed @
	dup a lshift xor
	dup b rshift xor
	dup c lshift xor
	dup seed! ;

hide{ a b c seed }hide

( ==================== Random Numbers ======================== )

( ==================== Prime Numbers ========================= )
( From original "third" code from the IOCCC at
http://www.ioccc.org/1992/buzzard.2.design, the module works
out and prints prime numbers. )

: prime? ( u -- u | 0 : return number if it is prime, zero otherwise )
	dup 1 = if 1- exit then
	dup 2 = if    exit then
	dup 2/ 2     ( loop from 2 to n/2 )
	do
		dup   ( value to check if prime )
		i mod ( mod by divisor )
		not if
			drop 0 leave
		then
	loop ;

0 variable counter

: primes ( x1 x2 -- : print the primes from x2 to x1 )
	0 counter !
	"  The primes from " dup . " to " over . " are: "
	cr
	column.reset
	do
		i prime?
		if
			i . counter @ column counter 1+!
		then
	loop
	cr
	"  There are " counter @ . " primes."
	cr ;

hide{ counter }hide
( ==================== Prime Numbers ========================= )

( ==================== Debugging info ======================== )
( This section implements various debugging utilities that the
programmer can use to inspect the environment and debug Forth
words. )

( String handling should really be done with PARSE, and CMOVE )
: sh ( cnl -- ior : execute a line as a system command )
	nl word count system ;

hide{ .s }hide
: .s    ( -- : print out the stack for debugging )
	[char] < emit depth (.) drop [char] > emit space
	depth if
		depth 0 do i column tab depth i 1+ - pick . loop
	then
	cr ;

1 variable hide-words ( do we want to hide hidden words or not )

: name ( PWD -- c-addr : given a pointer to the PWD field of a word get a pointer to the name of the word )
	dup 1+ @ 256/ word-mask and lsb - chars> ;

( This function prints out all of the defined words, excluding
hidden words.  An understanding of the layout of a Forth word
helps here. The dictionary contains a linked list of words,
each forth word has a pointer to the previous word until the
first word. The layout of a Forth word looks like this:

NAME:  Forth Word - A variable length ASCII NUL terminated
       string.
PWD:   Previous Word Pointer, points to the previous
       word.
CODE:  Flags, code word and offset from previous word
       pointer to start of Forth word string.
DATA:  The body of the forth word definition, not interested
       in this.

There is a register which stores the latest defined word which
can be accessed with the code "pwd @". In order to print out
a word we need to access a words CODE field, the offset to
the NAME is stored here in bits 8 to 14 and the offset is
calculated from the PWD field.

"print" expects a character address, so we need to multiply
any calculated address by the word size in bytes. )

: words.immediate ( bool -- : emit or mark a word being printed as being immediate )
	not if dark red foreground color then ;

: words.defined ( bool -- : emit or mark a word being printed as being a built in word )
	not if bright green background color then ;

: words.hidden ( bool -- : emit or mark a word being printed as being a hidden word )
	if dark magenta foreground color then ;

: words ( -- : print out all defined an visible words )
	latest
	space
	begin
		dup
		hidden? hide-words @ and
		not if
			hidden? words.hidden
			compiling? words.immediate
			dup defined-word? words.defined
			name
			print space
			reset-color
		else
			drop
		then
		@  ( Get pointer to previous word )
		dup dictionary-start u< ( stop if pwd no longer points to a word )
	until
	drop cr ;

( Simpler version of words
: words
	pwd @
	begin
		dup name print space @ dup dictionary-start u<
	until drop cr ; )

hide{ words.immediate words.defined words.hidden hidden? hidden-bit }hide

: TrueFalse ( bool -- : print true or false )
	if " true" else " false" then ;

: registers ( -- : print out important registers and information about the virtual machine )
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
	" tracing on:              " `debug   @ TrueFalse cr
	" starting word:           " `instruction ? cr
	" real start address:      " `start-address ? cr
	" error handling:          " `error-handler ? cr
	" throw handler:           " `handler ? cr
	" signal recieved:         " `signal ? cr ;
	
( `sin `sidx `slen `fout
 `stdout `stderr `argc `argv )


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

: step
	( step through a word: this word could be augmented
	with commands such as "dump", "halt", and optional
	".s" and "registers" )
	registers
	" .s: " .s cr
	" -- press any key to continue -- "
	key drop ;

: more ( -- : wait for more input )
	"  -- press any key to continue -- " key drop cr page ;

: debug-help ( -- : print out the help for the debug command )
 " debug mode commands
	h - print help
	q - exit interpreter word
	r - print registers
	s - print stack
	R - print return stack
	c - continue on with execution
" ;

doer debug-prompt
: prompt-default
	." debug> " ;

make debug-prompt prompt-default

: debug ( -- : enter interactive debug prompt )
	cr
	" Entered Debug Prompt. Type 'h' <return> for help. " cr
	begin
		key
		case
			nl     of debug-prompt endof
			[char] h of debug-help endof
			[char] q of bye        endof
			[char] r of registers  endof
			[char] s of .s         endof
			[char] R of r.s        endof
			[char] c of exit       endof
		endcase
	again ;
hide debug-prompt

: code>pwd ( CODE -- PWD/0 : calculate PWD from code address )
	dup dictionary-start here within not if drop 0 exit then
	1 cells - ;

: word-printer ( CODE -- : print out a words name given its code field )
	dup 1 cells - @ -1 = if . " noname" exit then ( nonames are marked by a -1 before its code field )
	dup code>pwd ?dup-if .d name print else drop " data" then
	        drop ;

hide{ code>pwd }hide

( these words push the execution tokens for various special
cases for decompilation )
: get-branch  [ find branch  ] literal ;
: get-?branch [ find ?branch ] literal ;
: get-original-exit [ find _exit ] literal ;
: get-quote   [ find ' ] literal ;

: branch-increment ( addr branch -- increment : calculate decompile increment for "branch" )
	1+ dup negative?
	if
		over cr . [char] : emit space . cr 2
	else
		2dup 2- nos1+ nos1+ dump
	then ;

( these words take a code field to a primitive they implement,
decompile it and any data belonging to that operation, and push
a number to increment the decompilers code stream pointer by )

: decompile-literal ( code -- increment )
	1+ ? " literal" 2 ;

: decompile-branch  ( code -- increment )
	dark red foreground color
	1+ ? " branch " dup 1+ @ branch-increment ;

: decompile-quote   ( code -- increment )
	dark green foreground color
	dup
	[char] ' emit 1+ @ word-printer 2 reset-color ;

: decompile-?branch ( code -- increment )
	1+ ? " ?branch" 2 ;

: decompile-exit ( code -- 0 )
	" _exit" cr " End of word:   " .  0 ;

( The decompile word expects a pointer to the code field of
a word, it decompiles a words code field, it needs a lot of
work however.  There are several complications to implementing
this decompile function.

	'	 The next cell should be pushed

	:noname  This has a marker before its code field of
	         -1 which cannot occur normally, this is
	         handled in word-printer

	branch	 branches are used to skip over data, but
	         also for some branch constructs, any data
	         in between can only be printed out
	         generally speaking

	exit	 There are two definitions of exit, the one
	         used in ';' and the one everything else uses,
	         this is used to determine the actual end
	         of the word

	literals Literals can be distinguished by their
	         low value, which cannot possibly be a word
	         with a name, the next field is the
	         actual literal

Of special difficult is processing IF, THEN and ELSE
statements, this will require keeping track of '?branch'.

Also of note, a number greater than "here" must be data )

: decompile ( code-pointer -- code-pointer increment|0 : )
	.d [char] : emit space dup @
	case
		dolit             of dup decompile-literal cr endof
		get-branch        of dup decompile-branch     endof
		get-quote         of dup decompile-quote   cr endof
		get-?branch       of dup decompile-?branch cr endof
		get-original-exit of dup decompile-exit       endof
		dup word-printer 1 swap cr
	endcase reset-color ;

: decompiler ( code-field-ptr -- : decompile a word in its entirety )
	begin decompile over + tuck = until drop ;

hide{
	word-printer get-branch get-?branch get-original-exit
	get-quote branch-increment decompile-literal
	decompile-branch decompile-?branch decompile-quote
	decompile-exit
}hide

( these words expect a pointer to the PWD field of a word )
: see.name         " name:          " name print cr ;
: see.start        " word start:    " name chars . cr ;
: see.previous     " previous word: " @ . cr ;
: see.immediate    " immediate:     " compiling? nip not TrueFalse cr ;
: see.instruction  " instruction:   " xt-instruction . cr ;
: see.defined      " defined:       " defined-word? TrueFalse cr ;

: see.header ( PWD -- is-immediate-word? )
	dup see.name
	dup see.start
	dup see.previous
	dup see.immediate
	dup see.instruction
	see.defined ;

( This does not work for all words. Specifically:

	2variable
	2constant
	table
	constant
	variable
	array 

Which are all complex CREATE words

A good way to test decompilation is with the following
Unix pipe:

	./forth -f forth.fth -e words
		| sed 's/ / see /g'
		| ./forth -t forth.fth &> decompiled.log
)

: see ( c" xxx" -- : decompile a word )
	find
	dup 0= if -32 throw then
	cell- ( move to PWD field )
	dup see.header
	dup defined-word?
	if ( decompile if a compiled word )
		2 cells + ( move to code field )
		" code field:" cr
		decompiler
	else ( the instruction describes the word if it is not a compiled word )
		dup 1 cells + @ instruction-mask and doconst = if ( special case for constants )
			" constant:      " 2 cells + @ .
		else
			drop
		then
	then cr ;

( NB. This has a bug, if any data within a word happens
to match the address of _exit word.end calculates the wrong
address, this needs to mirror the DECOMPILE word )
: word.end ( addr -- addr : find the end of a word )
	begin
		dup
		@ [ find _exit ] literal = if exit then
		cell+
	again ;

: (inline) ( xt -- : inline an word from its execution token )
	dup cell- defined-word? if
		cell+
		dup word.end over - here -rot dup allot move
	else
		,
	then ;

: ;inline ( -- : terminate :inline )
	immediate -22 throw ;

: :inline immediate
	?comp
	begin
		find found?
		dup [ find ;inline ] literal = if
			drop exit ( terminate :inline )
		then
		(inline)
	again ;

hide{
	see.header see.name see.start see.previous see.immediate
	see.instruction defined-word? see.defined _exit found?
	(inline) word.end
}hide

( These help messages could be moved to blocks, the blocks
could then be loaded from disk and printed instead of defining
the help here, this would allow much larger help )

: help ( -- : print out a short help message )
	page
	key drop
" Welcome to Forth, an imperative stack based language. It is
both a low level and a high level language, with a very small
memory footprint. Most of Forth is defined as a combination
of various primitives.

A short description of the available function (or Forth words)
follows, words marked (1) are immediate and cannot be used in
command mode, words marked with (2) define new words. Words
marked with (3) have both command and compile functionality.

"
more " Some of the built in words that accessible are:

(1,2)	:                 define a new word, switching to compile mode
	immediate         make latest defined word immediate
	read              read in a word, execute in command mode else compile
	@ !               fetch, store
	c@ c!             character based fetch and store
	- + * /           standard arithmetic operations,
	and or xor invert standard bitwise operations
	lshift rshift     left and right bit shift
	u< u> < > =       comparison predicates
	exit              exit from a word
	emit              print character from top of stack
	key               get a character from input
	r> >r             pop a value from or to the return stack
	find              find a word in the dictionary and push the location
	'                 store the address of the following word on the stack
	,                 write the top of the stack to the dictionary
	swap              swap first two values on the stack
	dup               duplicate the top of the stack
	drop              pop and drop a value
	over              copy the second stack value over the first
	.                 pop the top of the stack and print it
"
more "
	print             print a NUL terminated string at a character address
	depth             get the current stack depth
	clock             get the time since execution start in milliseconds
	evaluate          evaluate a string
	system            execute a system command
	close-file        close a file handle
	open-file         open a file handle
	delete-file       delete a file off disk given a string
	read-file         read in characters from a file
	write-file        write characters to a file
	file-position     get the file offset
	reposition-file   reposition the file pointer
	flush-file        flush a file to disk
	rename-file       rename a file on disk
 "

more " All of the other words in the interpreter are built
from these primitive words. A few examples:

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

" more "
For more information either consult the manual pages forth(1)
and libforth(1) or consult the following sources:

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

( ==================== Debugging info ======================== )

( ==================== Files ================================= )
( These words implement more of the standard file access words
in terms of the ones provided by the virtual machine. These
words are not the most easy to use words, although the ones
defined here and the ones that are part of the interpreter are
the standard ones.

Some thoughts:

I believe extensions to this word set, once I have figured
out the semantics, should be made to make them easier to use,
ones which automatically clean up after failure and call throw
would be more useful [and safer] when dealing with a single
input or output stream. The most common action after calling
one of the file access words is to call throw any way.

For example, a word like:

	: #write-file \ c-addr u fileid -- fileid u
		dup >r
		write-file dup if r> close-file throw throw then
		r> swap ;

Would be easier to deal with, it preserves the file identifier
and throws if there is any problem. It would be a bit more
difficult to use when there are two files open at a time.

The other file access methods could be implemented in terms of the
built in ones.

NB. read-line and write-line need their flag and ior setting
correctly

	FILE-SIZE    [ use file-positions ]

Also of note, Source ID needs extending.

See: [http://forth.sourceforge.net/std/dpans/dpans11.htm]  )

: read-char ( c-addr fileid -- ior : read a char )
	1 swap read-file 0<> swap 1 <> or ;

0 variable x

: getchar ( fileid -- char ior )
	x chars> swap read-char x chars> c@ swap ;

: write-char ( c-addr fileid -- ior : write a char )
	1 swap write-file 0<> swap 1 <> or ;

: putchar ( char fileid -- ior )
	swap x chars> c! x chars> swap write-char ;


hide{ x }hide

: rewind-file ( file-id -- : rewind a file to the beginning )
	0 reposition-file throw ;

: read-line ( c-addr u1 fileid -- u2 flag ior : read in a line of text )
	-rot bounds
	do
		dup i swap read-char drop
		i c@ nl = if drop i 0 0 leave then
	loop drop ;

: write-line  ( c-addr u fileid -- u2 flag ior : write a line of text )
	-rot bounds
	do
		dup i swap write-char drop
		i c@ nl = if drop i 0 0 leave then
	loop ;

: resize-file  ( ud fileid -- ior : attempt to resize a file )
	( There is no portable way to truncate a file :C )
	2drop -1 ( -1 to indicate failure ) ;

: create-file ( c-addr u fam -- fileid ior )
	>r 2dup w/o open-file throw close-file throw
	r> open-file ;

: include-file ( file-id -- : evaluate a file )
	dup >r 0 1 evaluator r> close-file throw throw ;

: included ( c-addr u -- : attempt to open up a name file and evaluate it )
	r/o open-file throw
	include-file ;

: include ( c" ccc" -- : attempt to evaluate a named file )
	( @bug requires trailing space, should use parse-name )
	bl word count included ;

: bin ( fam1 -- fam2 : modify a file access method to be binary not line oriented )
	( Do nothing, all file access methods are binary )
	;

( ==================== Files ================================= )

( ==================== Matcher =============================== )
( The following section implements a very simple regular
expression engine, which expects an ASCIIZ Forth string. It
is translated from C code and performs an identical function.

The regular expression language is as follows:

	c	match a literal character
	.	match any character
	*	match any characters

The "*" operator performs the same function as ".*" does in
most other regular expression engines. Most other regular
expression engines also do not anchor their selections to the
beginning and the end of the string to match, instead using
the operators '^' and '$' to do so, to emulate this behavior
'*' can be added as either a suffix, or a prefix, or both,
to the matching expression.

As an example "*, World!" matches both "Hello, World!" and
"Good bye, cruel World!". "Hello, ...." matches "Hello, Bill"
and "Hello, Fred" but not "Hello, Tim" as there are two few
characters in the last string. )

\ Translated from http://c-faq.com/lib/regex.html
\ int match(char *pat, char *str)
\ {
\ 	switch(*pat) {
\ 	case '\0':  return !*str;
\ 	case '*':   return match(pat+1, str) || *str && match(pat, str+1);
\ 	case '.':   return *str && match(pat+1, str+1);
\ 	default:    return *pat == *str && match(pat+1, str+1);
\ 	}
\ }

: *pat ( regex -- regex char : grab next character of pattern )
	dup c@ ;

: *str ( string regex -- string regex char : grab next character string to match )
	over c@ ;

: pass ( c-addr1 c-addr2 -- bool : pass condition, characters matched )
	2drop 1 ;

: reject ( c-addr1 c-addr2 -- bool : fail condition, character not matched )
	2drop 0 ;

: *pat==*str ( c-addr1 c-addr2 -- c-addr1 c-addr2 bool )
	2dup c@ swap c@ = ;

: ++ ( u1 u2 u3 u4 -- u1+u3 u2+u4 : not quite d+ [does no carry] )
	swap >r + swap r> + swap ;

defer matcher

: advance ( string regex char -- bool : advance both regex and string )
	if 1 1 ++ matcher else reject then ;

: advance-string ( string regex char -- bool : advance only the string )
	if 1 0 ++ matcher else reject then ;

: advance-regex ( string regex -- bool : advance matching )
	2dup 0 1 ++ matcher if pass else *str advance-string then ;

: match ( string regex -- bool : match a ASCIIZ pattern against an ASCIIZ string )
	*pat
	case
		       0 of drop c@ not   endof
		[char] * of advance-regex endof
		[char] . of *str advance  endof
		
		drop *pat==*str advance exit

	endcase ;

matcher is match

hide{
	*str *pat *pat==*str pass reject advance
	advance-string advance-regex matcher ++
}hide

( ==================== Matcher =============================== )


( ==================== Cons Cells ============================ )
( From http://sametwice.com/cons.fs, this could be improved
if the optional memory allocation words were added to
the interpreter. This provides a simple "cons cell" data
structure. There is currently no way to free allocated
cells. I do not think this is particularly useful, but it is
interesting. )

: car! ( value cons-addr -- : store a value in the car cell of a cons cell )
	! ;

: cdr! ( value cons-addr -- : store a value in the cdr cell of a cons cell )
	cell+ ! ;

: car@ ( cons-addr -- car-val : retrieve car value from cons cell )
	@ ;

: cdr@ ( cons-addr -- cdr-val : retrieve cdr value from cons cell )
	cell+ @ ;

: cons ( car-val cdr-val -- cons-addr : allocate a new cons cell )
	swap here >r , , r> ;

: cons0 0 0 cons ;

( ==================== Cons Cells ============================ )

( ==================== License =============================== )
( The license has been chosen specifically to make this library
and any associated programs easy to integrate into arbitrary
products without any hassle. For the main libforth program
the LGPL license would have been also suitable [although it
is MIT licensed as well], but to keep confusion down the same
license, the MIT license, is used in both the Forth code and
C code. This has not been done for any ideological reasons,
and I am not that bothered about licenses. )

: license ( -- : print out license information )
"
The MIT License (MIT)

Copyright (c) 2016, 2017 Richard James Howe

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the 'Software'), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

"
;

( ==================== License =============================== )

( ==================== Core utilities ======================== )
( Read the header of a core file and process it, printing the
results out )

8 constant header-size   ( size of Forth core file header )
8 constant size-field-size ( the size in bytes of the size field in the core file )
0 variable core-file      ( core fileid we are reading in )
0 variable core-cell-size ( cell size of Forth core )
0 variable core-version   ( version of core file )
0 variable core-endianess ( endianess of core we are reading in )

( save space to read in header )
create header header-size chars allot
: cheader ( -- c-addr : header char address )
	header chars> ;
create size-field size-field-size chars allot
: csize-field ( -- c-addr : address of place size field is stored in )
	size-field chars> ;

0
enum header-magic0     ( magic number 0 : FF )
enum header-magic1     ( magic number 1 : '4' )
enum header-magic2     ( magic number 2 : 'T' )
enum header-magic3     ( magic number 3 : 'H' )
enum header-cell-size  ( size of a forth cell, either 2, 4 or 8 bytes )
enum header-version    ( version of the forth core )
enum header-endianess  ( endianess of the core )
enum header-log2size   ( binary logarithm of the core size )

: cleanup ( -- : cleanup before abort )
	core-file @ ?dup 0<> if close-file drop then ;

: invalid-header ( bool -- : abort if header is invalid )
	<> if cleanup abort" invalid header" then ;

: save-core-cell-size ( char -- : save the core file cell size, checking if it is valid )
	core-cell-size !
	" cell size:" tab
	core-cell-size @ 2 = if 2 . cr exit then
	core-cell-size @ 4 = if 4 . cr exit then
	core-cell-size @ 8 = if 8 . cr exit then
	cleanup core-cell-size @ . abort" : invalid cell size"  ;

: check-version-compatibility ( char -- : checks the version compatibility of the core file )
	core-version !
	core-version @ version = if " version:        " version . cr exit then
	cleanup core-version @ . abort" : unknown version number" ;

: save-endianess ( char -- : save the endianess, checking if it is valid )
	core-endianess !
	" endianess:" tab
	core-endianess @ 0 = if " big"    cr exit then
	core-endianess @ 1 = if " little" cr exit then
	cleanup core-endianess @ . abort" invalid endianess" then ;

: read-or-abort ( c-addr size fileid -- : )
	over >r read-file
	  0<> if cleanup abort" file read failed" then
	r> <> if cleanup abort" header too small" then ;

: header? ( -- : print out header information )
	cheader header-size core-file @ read-or-abort
	( " raw header:" header 2 dump )
	cheader header-magic0    + c@      255 invalid-header
	cheader header-magic1    + c@ [char] 4 invalid-header
	cheader header-magic2    + c@ [char] T invalid-header
	cheader header-magic3    + c@ [char] H invalid-header
	cheader header-cell-size + c@ save-core-cell-size
	cheader header-version   + c@ check-version-compatibility
	cheader header-endianess + c@ save-endianess
	" valid header" cr ;

: size? ( -- : print out core file size )
	" size:           " cheader header-log2size + c@ 1 swap lshift . cr ;

: core ( c-addr u -- : analyze a Forth core file from disk given its file name )
	2dup " core file:" tab type cr
	r/o open-file throw core-file !
	header?
	size?
	core-file @ close-file drop ;

( s" forth.core" core )

hide{
header-size header?
header-magic0 header-magic1 header-magic2 header-magic3
header-version header-cell-size header-endianess header-log2size
header
core-file save-core-cell-size check-version-compatibility
core-cell-size cheader
core-endianess core-version save-endianess invalid-header
cleanup size-field csize-field size-field-size
read-or-abort size?
}hide

( ==================== Core utilities ======================== )

( ==================== RLE =================================== )

( These set of words implement Run Length Compression, which
can be used for saving space when compressing the core files
generated by Forth programs, which contain mostly runs of
NUL characters.

The format of the encoded data is quite simple, there is a
command byte followed by data. The command byte encodes only
two commands; encode a run of literal data and repeat the
next character.

If the command byte is greater than X the command is a run
of characters, X is then subtracted from the command byte
and this is the number of characters that is to be copied
verbatim when decompressing.

If the command byte is less than or equal to X then this
number, plus one, is used to repeat the next data byte in
the input stream.

X is 128 for this application, but could be adjusted for
better compression depending on what the data looks like.

Example:

	2 'a' 130 'b' 'c' 3 'd'

Becomes:

	aabcddd

Example usage:

	: extract
		c" forth.core" w/o open-file throw c"
		forth.core.rle" r/o open-file throw
		decompress ;
	extract

File redirection could be used for the input as well )

: cpad pad chars> ;

0 variable out
128 constant run-length

: next.char ( file-id -- char : read in a single character )
	>r cpad r> read-char throw cpad c@ ;

: repeated ( count file-id -- : repeat a character count times )
	next.char swap 0 do dup emit loop drop ;

: literals ( count file-id -- : extract a literal run )
	>r cpad swap r> read-file throw cpad swap type ;

: command ( file-id -- : process an RLE command )
	dup
	>r next.char
	dup run-length u>
	if
		run-length - r> literals
	else
		1+ r> repeated
	then ;

: redirect ( file-id-out -- : save current output pointer, redirect to output )
	`fout @ out ! `fout ! ;

: restore ( -- : restore previous output pointer )
	out @ `fout ! ;

: decompress ( file-id-out file-id-in -- : decompress an RLE encoded file )
	swap
	redirect
	begin dup ['] command catch until ( process commands until input exhausted )
	2drop ( drop twice because catch will restore stack before COMMAND )
	restore ; ( restore input stream )

hide{ literals repeated next.char out run-length command }hide

( ==================== RLE =================================== )

( ==================== Generate C Core file ================== )
( The word core2c reads in a core file and turns it into a
C file which can then be compiled into the forth interpreter
in a bootstrapping like process. The process is described
elsewhere as well, but it goes like this.

1. We want to generate an executable program that contains the
core Forth interpreter, written in C, and the contents of this
file in compiled form. However, in order to compile this code
we need a Forth interpreter to do so. This is the problem.
2. To solve the problem we first make a program which is capable
of compiling "forth.fth".
3. We then generate a core file from this.
4. We then convert the generated core file to C code.
5. We recompile the original program to includes this core
file, and which runs the core file at start up.
6. We run the new executable.

Usage:
 c" forth.core" c" core.gen.c" core2c )

0 variable count

: wbyte ( u char -- : write a byte )
	(.) drop
	[char] , emit
	16 mod 0= if cr then ;

: advance ( char -- : advance counter and print byte )
	count 1+! count @ swap wbyte ;

: hexify ( fileid -- fileid : turn core file into C numbers in array )
	0 count !
	begin dup getchar 0= while advance repeat drop ;

: quote ( -- : emit a quote character )
	[char] " emit ;

: core2c ( c-addr u c-addr u -- ior : generate a C file from a core file )
	w/o open-file throw >r
	r/o open-file ?dup-if r> close-file throw throw then
	r> redirect
	" #include " quote " libforth.h" quote cr
	" unsigned char forth_core_data[] = {" cr
	hexify
	" };" cr
	" forth_cell_t forth_core_size = " count @ . " ;" cr cr
	close-file
	`fout @ close-file
	restore or ;

hide{ wbyte hexify count quote advance }hide

( ==================== Generate C Core file ================== )

( ==================== Word Count Program ==================== )

: wc ( c-addr u -- u : count the bytes in a file )
	r/o open-file throw
	0 swap
	begin
		dup getchar nip 0=
	while
		nos1+
	repeat
	close-file throw ;

( ==================== Word Count Program ==================== )

( ==================== Save Core file ======================== )
( The following functionality allows the user to save the
core file from within the running interpreter. The Forth
core files have a very simple format which means the words
for doing this do not have to be too long, a header has to
emitted with a few calculated values and then the contents
of the Forths memory after this )

( This write the header out to the current output device, this
will be redirected to a file )
: header  ( -- : write the header out )
	0xff          emit ( magic 0 )
	[char] 4      emit ( magic 1 )
	[char] T      emit ( magic 2 )
	[char] H      emit ( magic 3 )
	size          emit ( cell size in bytes )
	version       emit ( core version )
	endian not    emit ( endianess )
	max-core log2 emit ; ( size field )

: data ( -- : write the data out )
	0 max-core chars> `fout @ write-file throw drop ;

: encore ( -- : write the core file out )
	header
	data ;

: save-core ( c-addr u -- : save core file or throw error )
	w/o open-file throw dup
	redirect
		['] encore catch swap close-file throw
	restore ;

( The following code illustrates an example of setting up a
Forth core file to execute a word when the core file is loaded.
In the example the word "hello-world" will be executed,
which will also quit the interpreter:

This only works for immediate words for now, we define
the word we wish to be executed when the forth core
is loaded:

	: hello-world immediate
		" Hello, World!" cr bye ;

The following sets the starting word to our newly
defined word:

	find hello-world cfa start!

	\ Now we can save the core file out:
	s" forth.core" save-core

This can be used, in conjunction with aspects of the build
system, to produce a standalone executable that will run only
a single Forth word. This is known as creating a 'turn-key'
application. )

hide{ redirect restore data encore header }hide

( ==================== Save Core file ======================== )

( ==================== Date ================================== )
( This word set implements a words for date processing, so
you can print out nicely formatted date strings. It implements
the standard Forth word time&date and two words which interact
with the libforth DATE instruction, which pushes the current
time information onto the stack. )


: >month ( month -- c-addr u : convert month to month string )
	case
		 1 of c" Jan " endof
		 2 of c" Feb " endof
		 3 of c" Mar " endof
		 4 of c" Apr " endof
		 5 of c" May " endof
		 6 of c" Jun " endof
		 7 of c" Jul " endof
		 8 of c" Aug " endof
		 9 of c" Sep " endof
		10 of c" Oct " endof
		11 of c" Nov " endof
		12 of c" Dec " endof
		-11 throw
	endcase ;

: .day ( day -- c-addr u : add ordinal to day )
	10 mod
	case
		1 of c" st " endof
		2 of c" nd " endof
		3 of c" rd " endof
		drop c" th " exit
	endcase ;

: >day ( day -- c-addr u: add ordinal to day of month )
	dup  1 10 within if .day   exit then
	dup 10 20 within if drop c" th " exit then
	.day ;

: >weekday ( weekday -- c-addr u : print the weekday )
	case
		0 of c" Sun " endof
		1 of c" Mon " endof
		2 of c" Tue " endof
		3 of c" Wed " endof
		4 of c" Thu " endof
		5 of c" Fri " endof
		6 of c" Sat " endof
		-11 throw
	endcase ;

: >gmt ( bool -- GMT or DST? )
	if c" DST " else c" GMT " then ;

: colon ( -- char : push a colon character )
	[char] : ;

: 0? ( n -- : hold a space if number is less than base )
	(base) u< if [char] 0 hold then ;

( .NB You can format the date in hex if you want! )
: date-string ( date -- c-addr u : format a date string in transient memory )
	9 reverse ( reverse the date string )
	<#
		dup #s drop 0? ( seconds )
		colon hold
		dup #s drop 0? ( minute )
		colon hold
		dup #s drop 0? ( hour )
		dup >day holds
		#s drop ( day )
		>month holds
		bl hold
		#s drop ( year )
		>weekday holds
		drop ( no need for days of year )
		>gmt holds
		0
	#> ;

: .date ( date -- : print the date )
	date-string type ;

: time&date ( -- second minute hour day month year )
	date
	3drop ;

hide{ >weekday .day >day >month colon >gmt 0? }hide

( ==================== Date ================================== )


( ==================== CRC =================================== )

( Make a word to limit arithmetic to a 16-bit value )
size 2 = [if]
	: limit immediate ;  ( do nothing, no need to limit )
[else]
	: limit 0xffff and ; ( limit to 16-bit value )
[then]

: ccitt ( crc c-addr -- crc : calculate polynomial 0x1021 AKA "x16 + x12 + x5 + 1" )
	c@                         ( get char )
	limit over 256/ xor        ( crc x )
	dup  4  rshift xor         ( crc x )
	dup  5  lshift limit xor   ( crc x )
	dup  12 lshift limit xor   ( crc x )
	swap 8  lshift limit xor ; ( crc )

( See http://stackoverflow.com/questions/10564491
  and https://www.lammertbies.nl/comm/info/crc-calculation.html )
: crc16-ccitt ( c-addr u -- u )
	0xffff -rot
	['] ccitt foreach ;
hide{ limit ccitt }hide

( ==================== CRC =================================== )

( ==================== Rational Data Type ==================== )
( This word set allows the manipulation of a rational data
type, which are basically fractions. This allows numbers like
1/3 to be represented without any loss of precision. Conversion
to and from the data type to an integer type is trivial,
although information can be lost during the conversion.

To convert to a rational, use DUP, to convert from a
rational, use '/'.

The denominator is the first number on the stack, the numerator
the second number. Fractions are simplified after any rational
operation, and all rational words can accept unsimplified
arguments. For example the fraction 1/3 can be represented as
6/18, they are equivalent, so the rational equality operator
"=rat" can accept both and returns true.

	T{ 1 3 6 18 =rat -> 1 }T

See: https://en.wikipedia.org/wiki/Rational_data_type For
more information.

This set of words use two cells to represent a fraction,
however a single cell could be used, with the numerator and the
denominator stored in upper and lower half of a single cell. )

: simplify ( a b -- a/gcd{a,b} b/gcd{a/b} : simplify a rational )
  2dup
  gcd
  tuck
  /
  -rot
  /
  swap ; \ ? check this

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
  2swap ; ( done! )

: *rat ( a/b c/d -- a/b : multiply two rationals together )
  rot * -rot * swap simplify ;

: /rat ( a/b c/d -- a/b : divide one rational by another )
  swap *rat ;

: +rat ( a/b c/d -- a/b : add two rationals together )
  crossmultiply
  rot
  drop ( or check if equal, if not there is an error )
  -rot
  +
  swap
  simplify ;

: -rat ( a/b c/d -- a/b : subtract one rational from another )
  crossmultiply
  rot
  drop ( or check if equal, if not there is an error )
  -rot
  -
  swap
  simplify ;

: .rat ( a/b --  : print out a rational number )
  simplify swap (.) drop [char] / emit . ;

: =rat ( a/b c/d -- bool : rational equal )
  crossmultiply rot = -rot = = ;

: >rat ( a/b c/d -- bool : rational greater than )
  crossmultiply rot 2drop > ;

: <=rat ( a/b c/d -- bool : rational less than or equal to )
	>rat not ;

: <rat ( a/b c/d -- bool : rational less than )
  crossmultiply rot 2drop < ;

: >=rat ( a/b c/d -- bool : rational greater or equal to )
	<rat not ;

( >rational is a work in progress, make it better )
: 0>number 0 -rot >number ;
0 0 2variable saved
: failed 0 0 saved 2@ ;
: >rational ( c-addr u -- a/b c-addr u )
	2dup saved 2!
	0>number 2dup 0= if 4drop failed exit then ( Could convert to rational n/1 )
	c@ [char] / <> if 3drop failed exit then
	1 /string
	0>number ;

hide{ 0>number saved failed }hide

( ==================== Rational Data Type ==================== )

( ==================== Block Layer =========================== )
( This is the block layer, it assumes that the file access
words exists and use them, it would have to be rewritten
for an embedded device that used EEPROM or something
similar. Currently it does not interact well with the current
input methods used by the interpreter which will need changing.

The block layer is the traditional way Forths implement a
system to interact with mass storage, one which imposes little
on the underlying system only requiring that blocks 1024 bytes
can be loaded and saved to it [which make it suitable for
microcomputers that lack a file system or an embedded device].

The block layer is used less than it once as a lot more Forths
are hosted under a guest operating system so have access to
methods for reading and writing to files through it.

Each block number accepted by BLOCK is backed by a file
[or created if it does not exist]. The name of the file is
the block number with ".blk" appended to it.

Some Notes:

BLOCK only uses one block buffer, most other Forths have
multiple block buffers, this could be improved on, but
would take up more space.

Another way of storing the blocks could be made, which is to
store the blocks in a single file and seek to the correct
place within it. This might be implemented in the future,
or offered as an alternative. 

Yet another way is to not have on disk blocks, but instead
have in memory blocks, this simplifies things significantly,
and would mean saving the blocks to disk would use the same
mechanism as saving the core file to disk. Computers certainly
have enough memory to do this. The block word set could
be factored so it could use either the on disk method or
the memory option. 

To simplify the current code, and make the code portable
to devices with EEPROM an instruction in the virtual machine
could be made which does the task of transfering a block to
disk. )

0 variable dirty
b/buf string buf  ( block buffer, only one exists )
0 , ( make sure buffer is NUL terminated, just in case )
0 variable blk ( 0 = invalid block number, >0 block number stored in buf)

: invalid? ( n -- : throw if block number is invalid )
	0= if -35 throw then ;

: update ( -- : mark currently loaded block buffer as dirty )
	true dirty ! ;

: updated? ( n -- bool : is a block updated? )
 	blk @ <> if 0 else dirty @ then ;

: block.name ( n -- c-addr u : make a block name )
	c" .blk" <# holds #s #> rot drop ;

( This will not work if we do not have permission,
or in various other cases where we cannot open the file,
for whatever reason )
: file-exists ( c-addr u : does a file exist? )
	r/o open-file if drop 0 else close-file throw 1 then ;

: block.exists ( n -- bool : does a block buffer exist on disk? )
	block.name file-exists ;

( block.write and block.read do not check if they have
wrote or read in 1024 bytes, nor do they check that they can
only write or read 1024 and not a byte more )

: block.read ( file-id -- file-id : read in buffer )
	dup >r buf r> read-file nip if close-file -33 throw then ;

: block.write ( file-id -- file-id : write out buffer )
	dup >r buf r> write-file nip if close-file -34 throw then ;

: block.open ( n fam -- file-id )
	>r block.name r> open-file throw ;

: save-buffers
	blk @ 0= if exit then    ( not a valid block number, exit )
	dirty @ not if exit then ( not dirty, no need to save )
	blk @ w/o block.open     ( open file backing block buffer )
	block.write              ( write it out )
	close-file throw         ( close it )
	false dirty ! ;          ( but only mark it clean if everything succeeded )

: empty-buffers ( -- : deallocate any saved buffers )
	0 blk ! ;

: flush ( -- : perform save-buffers followed by empty-buffers )
	save-buffers
	empty-buffers ;

( Block is a complex word that does a lot, although it has
a simple interface. It does the following given a block number:

1. Checks the provided block buffer number to make sure it
is valid.
2. If the block is already loaded from the disk, then return
the address of the block buffer it is loaded into.
3. If not, it checks to see if the currently loaded block
buffer is dirty, if it is then it flushes the buffer to disk.
4. If the block buffer does not exists on disk then it
creates it.
5. It then stores the block number in blk and returns an
address to the block buffer. )
: block ( n -- c-addr : load a block )
	dup invalid?
	dup blk @ = if drop buf drop exit then
	flush
	dup block.exists if        ( if the buffer exits on disk load it in )
		dup r/o block.open
		block.read
		close-file throw
	else                        ( else it does not exist )
		buf 0 fill          ( clean the buffer )
	then
	blk !                       ( save the block number )
	buf drop ;

: buffer block ;

: load ( n -- : load and execute a block )
	block b/buf evaluate throw ;

: +block ( n -- u : calculate new block number relative to current block )
	blk @ + ;

: --> ( -- : load next block )
	1 +block load ;

: blocks.make ( n1 n2 -- : make blocks on disk from n1 to n2 inclusive )
	1+ swap do i block b/buf bl fill update loop save-buffers ;

: block.copy ( n1 n2 -- bool : copy block n2 to n1 if n2 exists )
	swap dup block.exists 0= if 2drop false exit then ( n2 n1 )
	block drop ( load in block n1 )
	w/o block.open block.write close-file throw
	true ;

: block.delete ( n -- : delete block )
	dup block.exists 0= if drop exit then
	block.name delete-file drop ;

hide{
	block.name invalid? block.write
	block.read block.exists block.open dirty
}hide

( ==================== Block Layer =========================== )

( ==================== List ================================== )
( The list word set allows the viewing of Forth blocks,
this version of list can create two Formats, a simple mode
where it just spits out the block with a carriage return
after each line and a more fancy version which also prints
out line numbers and draws a box around the data. LIST is
not aware of any formatting and characters that might be
present in the data, or none printable characters.

A very primitive version of list can be defined as follows:

	: list block b/buf type ;

)

1 variable fancy-list
0 variable scr
64 constant c/l ( characters per line )

: pipe ( -- : emit a pipe character  )
	[char] | emit ;

: line.number ( n -- : print line number )
	fancy-list @ not if drop exit then
	2 u.r space pipe ;

: list.end ( -- : print the right hand side of the box )
	fancy-list @ not if exit then
	pipe ;

: line ( c-addr -- c-addr u : given a line number, display that line number and calculate offset )
	dup
	line.number ( display line number )
	c/l * +     ( calculate offset )
	c/l ;       ( add line length )

: list.type ( c-addr u -- : list a block )
	b/buf c/l / 0 do dup i line type list.end cr loop drop ;

: list.border ( -- : print a section of the border )
	" +---|---" ;

: list.box ( )
	fancy-list @ not if exit then
	4 spaces
	8 0 do list.border loop cr ;

: list ( n -- : display a block number and update scr )
	dup >r block r> scr ! list.box list.type list.box ;

: thru
	key drop
	1+ swap do i list more loop ;

hide{
	buf line line.number list.type
	(base) list.box list.border list.end pipe
}hide

( ==================== List ================================== )

( ==================== Signal Handling ======================= )
( Signal handling at the moment is quite primitive. When
a signal occurs it has to be explicitly tested for by the
programmer, this could be improved on quite a bit. One way
of doing this would be to check for signals in the virtual
machine and cause a THROW from within it. )

( signals are biased to fall outside the range of the error
numbers defined in the ANS Forth standard. )
-512 constant signal-bias

: signal ( -- signal/0 : push the results of the signal register )
	`signal @
 	0 `signal ! ;

( ==================== Signal Handling ======================= )

( Looking at most Forths dictionary with "words" command they
tend to have a lot of words that do not mean anything but to
the implementers of that specific Forth, here we clean up as
many non standard words as possible. )
hide{
 do-string ')' alignment-bits
 dictionary-start hidden-mask instruction-mask immediate-mask compiling?
 compile-bit
 max-core dolist doconst x x! x@
 max-string-length
 evaluator
 TrueFalse >instruction
 xt-instruction
 (bye)
 `source-id `sin `sidx `slen `start-address `fin `fout `stdin
 `stdout `stderr `argc `argv `debug `invalid `top `instruction
 `stack-size `error-handler `handler _emit `signal `x
}hide

(
## Forth To List

The following is a To-Do list for the Forth code itself,
along with any other ideas.

* FORTH, VOCABULARY

* "Value", "To", "Is"

* Double cell words

* The interpreter should use character based addresses,
instead of word based, and use values that are actual valid
pointers, this will allow easier interaction with the world
outside the virtual machine

* common words and actions should be factored out to simplify
definitions of other words, their standards compliant version
found if any.

* A soft floating point library would be useful which could be
used to optionally implement floats [which is not something I
really want to add to the virtual machine]. If floats were to
be added, only the minimal set of functions should be added
[f+,f-,f/,f*,f<,f>,>float,...]

* Allow the processing of argc and argv, the mechanism by which
that this can be achieved needs to be worked out. However all
processing that is currently done in "main.c" should be done
within the Forth interpreter instead. Words for manipulating
rationals and double width cells should be made first.

* A built in version of "dump" and "words" should be added
to the Forth starting vocabulary, simplified versions that
can be hidden.

* Here documents, string literals. Examples of these can be
found online

* Document the words in this file and built in words better,
also turn this document into a literate Forth file.

* Sort out "'", "[']", "find", "compile,"

* Proper booleans should be used throughout, that is -1 is
true, and 0 is false.

* Attempt to add crypto primitives, not for serious use,
like TEA, XTEA, XXTEA, RC4, MD5, ...

* Add hash functions: CRC-32, CRC-16, ...
http://stackoverflow.com/questions/10564491/

* Implement as many things from
http://lars.nocrew.org/forth2012/implement.html as is sensible.

* Works like EMIT and KEY should be DOER/MAKE words

* The current words that implement I/O redirection need to
be improved, and documented, I think this is quite a useful
and powerful mechanism to use within Forth that simplifies
programs. This is a must and will make writing utilities in
Forth a *lot* easier

* Words for manipulating words should be added, for navigating
to different fields within them, to the end of the word,
etcetera.

* The data structure used for parsing Forth words needs
changing in libforth so a counted string is produced. Counted
strings should be used more often. The current layout of a
Forth word prevents a counted string being used and uses a
byte more than it has to.

* Whether certain simple words [such as '1+', '1-', '>',
'<', '<>', NOT, <=', '>='] should be added as virtual
machine instructions for speed [and size] reasons should
be investigated.

* An analysis of the interpreter and the code it executes
could be done to find the most commonly executed words
and instructions, as well as the most common two and three
sequences of words and instructions. This could be used to use
to optimize the interpreter, in terms of both speed and size.

* The source code for this file should go through a code
review, which should focus on formatting, stack comments and
reorganizing the code. Currently it is not clear that variables
like COLORIZE and FANCY-LIST exist and can be changed by
the user. Lines should be 64 characters in length - maximum,
including the new line at the end of the file.

### libforth.c todo

* A halt, a parse, and a print/type instruction could be
added to the Forth virtual machine.

* Make case sensitivity optional

* u.r, or a more generic version should be added to the
interpreter instead of the current simpler primitive. As
should >number - for fast numeric input and output.

* Throw/Catch need to be added and used in the virtual machine

* Integrate Travis Continous Integration into the Github
project.

* A potential optimization is to order the words in the
dictionary by frequency order, this would mean chaning the
X Macro that contains the list of words, after collecting
statistics. This should make find faster.

* Investigate adding operating system specific code into the
interpreter and isolating it to make it semi-portable.

* Make equivalents for various Unix utilities in Forth,
like a CRC check, cat, tr, etcetera.

* It would be interesting to make a primitive file system based
upon Forth blocks, this could then be ported to systems that
do not have file systems, such as microcontrollers [which
usually have EEPROM].

* In a _very_ unportable way it would be possible to have an
instruction that takes the top of the stack, treats it as a
function pointer and then attempts to call said function. This
would allow us to assemble machine dependant code within the
core file, generate a new function, then call it. It is just
a thought.

* The code in this file needs to be rethought and simplified,
it sometimes does things in very odd, or suboptimal ways, this
mainly applies to:
 - The lack of vocabularies 
 - Input parsing and handling
 - Evaluation

* A way of generating standalone Forth executables should
be made, they should try to be as small as possible. This
might actually make Forth practical as an application programming
langauge, the programs could be compiled to C as an intermediate
step perhaps. The C load and save functions would need to be
modified.
 )

verbose [if]
	.( FORTH: libforth successfully loaded.) cr
	date .date cr
	.( Type 'help' and press return for a basic introduction.) cr
	.( Core: ) here . " / " here unused + . cr
	 license
[then]

( ==================== Test Code ============================= )

( The following will not work as we might actually be reading
from a string [`sin] not `fin.

: key 32 chars> 1 `fin @
	read-file drop 0 = if 0 else 32 chars> c@ then ; )

\ : ' immediate state @ if postpone ['] else find then ;

( This really does not implement a correct FORTH/VOCABULARY,
for that wordlists will need to be introduced and used in
libforth.c. The best that this word set can do is to hide
and reveal words to the user, this was just an experiment.

	: forth
		[ find forth 1- @ ] literal
		[ find forth 1- ] literal ! ;

	: vocabulary
		create does> drop 0 [ find forth 1- ] literal ! ; )

( ==================== Test Code ============================= )

( ==================== Error checking ======================== )
( This is a series of redefinitions that make the use of control
structures a bit safer. )

: if    immediate ?comp postpone if    [ hide if ]    ;
: else  immediate ?comp postpone else  [ hide else ]  ;
: then  immediate ?comp postpone then  [ hide then ]  ;
: begin immediate ?comp postpone begin [ hide begin ] ;
: until immediate ?comp postpone until [ hide until ] ;
: never immediate ?comp postpone never [ hide never ] ;

( ==================== Error checking ======================== )

( ==================== DUMP ================================== )
\ : newline ( x -- x+1 : print a new line every fourth value )
\ 	dup 3 and 0= if cr then 1+ ;
\
\ : address ( num count -- count : print current address we are dumping every fourth value )
\ 	dup >r
\ 	1- 3 and 0= if . [char] : emit space else drop then
\ 	r> ;
\
\ : dump ( start count -- : print the contents of a section of memory )
\ \	hex       ( switch to hex mode )
\ 	1 >r      ( save counter on return stack )
\ 	over + swap ( calculate limits: start start+count )
\ 	begin
\ 		2dup u> ( stop if gone past limits )
\ 	while
\ 		dup r> address >r
\ 		dup @ . 1+
\ 		r> newline >r
\ 	repeat
\ 	r> drop
\ 	2drop ;
\
\ hide newline
\ hide address
( ==================== DUMP ================================== )

( ==================== End of File Functions ================= )

( set up a rendezvous point, we can call the word RETREAT to
restore the dictionary to this point. This word also updates
fence to this location in the dictionary )
rendezvous

: task ; ( Task is a word that can safely be forgotten )

( ==================== End of File Functions ================= )

