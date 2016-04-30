#!./forth

( )

: logical ( x -- bool ) not not ;
: hidden? 0x80 and logical ;
: dictionary-start 
	( The dictionary start at this location, anything before this value 
	is not a defined word ) 
	64 
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
: 2+ ( x -- x ) 2 + ;
: 3+ ( x -- x ) 3 + ;
: hide ( WORD -- hide-token ) 
	( This hides a word from being found by the interpreter )
	find 1+ dup 
	if 
		dup @ 0x80 or swap tuck ! 
	else 
		drop 0 
	then 
;

: unhide ( hide-token -- ) dup @ 0x80 invert and swap ! ;
hide find-) drop 

: push-location 2 ;    ( location of special "push" word )
: run-instruction 2 ;  ( run code word )
: state-flag 8 ;       ( compile flag register location, used in "state" )
: ?dup ( x -- ? ) dup if dup then ;
: min ( x y -- min ) 2dup < if drop else swap drop then  ; 
: max ( x y -- max ) 2dup > if drop else swap drop then  ; 
: >= ( x y -- bool ) < not ;
: <= ( x y -- bool ) > not ;
: 2* ( x -- x ) 1 lshift ;
: 2/ ( x -- x ) 1 rshift ;
: #  ( x -- x ) dup . cr ;

: usleep * clock + begin dup clock < until drop ;
: sleep 1000 * usleep ;

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
	: create :: 2 , here 2 + , ' exit , 0 state ; 
But this version is much more limited )

: write-quote ' ' , ;   ( A word that writes ' into the dictionary )
: write-comma ' , , ;   ( A word that writes , into the dictionary )
: write-exit ' exit , ; ( A word that write exit into the dictionary ) 

: _create             \ create a new work that pushes its data field
	::            \ compile a word
	push-location ,       \ write push into new word
	here 2+ ,     \ push a pointer to data field
	' exit ,      \ write in an exit to new word data field is after exit 
	false state   \ return to command mode
;

: create immediate              \ This is a complicated word! It makes a
                                \ word that makes a word.
  state-flag @ if               \ Compile time behavior
  ' :: ,                        \ Make the defining word compile a header
  write-quote push-location , write-comma    \ Write in push to the creating word
  ' here , ' 3+ , write-comma        \ Write in the number we want the created word to push
  write-quote here 0 , write-comma   \ Write in a place holder 0 and push a 
                                     \ pointer to to be used by does>
  write-quote write-exit write-comma \ Write in an exit in the word we're compiling.
  ' false , ' state ,           \ Make sure to change the state back to command mode
  else                          \ Run time behavior
        _create
  then
;

: does> ( whole-to-patch -- ) 
  immediate
  write-exit                    \ Write in an exit, we don't want the
                                \ defining to run it, but the *defined* word to.
  here swap !                   \ Patch in the code fields to point to.
  run-instruction ,             \ Write a run in.
;

hide _create drop
hide write-quote drop
hide write-comma drop
hide write-exit drop

: array    create allot does> + ;
: variable create ,     does>   ;
: constant create ,     does> @ ;

( === End Create === )

0 variable i
0 variable j

: i! i ! ;
: j! j ! ;
: i@ i @ ;
: j@ j @ ;

( do loop : This section as the do-loop looping constructs )

: do immediate 
  ' j! ,
  ' i! ,
  here
;

: >= 
  < not
;

: ++i>=j
  i@ 1+ i! i@ j@ >= 
;

: loop immediate
  ' ++i>=j ,
  ' jz ,
  here - ,
;

hide ++i>=j drop

: break 
  j@ i!
;

: range   ( nX nY -- nX nX+1 ... nY ) 1+ do i@ loop ;
: repeat  ( n0 X -- n0 ... nX ) 0      swap 1- do dup loop ;
: sum 0   ( n0 ... nX X -- sum<0..X> ) swap 1- do + loop ;
: mul 0   ( n0 ... nX X -- mul<0..X> ) swap 1- do * loop ;

: factorial ( n -- n! ) 
  dup 1 
  <= if 
    1 
  else 
    dup >r 1 swap range r> mul 
  then 
;

( : 8* 8 * ;
: mask 8* 255 swap lshift ;
: size-1 [ size 1- literal ] ;
: alignment-bits size-1 and ;
: caddr [ size log-2 literal ] rshift ;
: c@ dup caddr @ swap alignment-bits tuck mask and swap 8* rshift ;
: c! tuck dup caddr @ swap alignment-bits rot swap  8* lshift or swap caddr ! ;
1 hex
1 0x4000 !
0xF0 0x8000 c! 
0xC0 0x8001 c!
0x4000 @ . cr )

\ ============================================================================
\ UTILITY
\ ============================================================================


