#!./forth 

marker unit-test-framework

( @todo These tests fail to run on a 32-bit platform, if "unit(" is used )

( Forth Unit Tests
This fill contains the tests for words and functionality defined in
forth.fth. The mechanism to test whether the tests have tested would
include looking at the core dump generated from executing this file, if
the forth interpreter has been invalidated then the tests have tested. The
core will not be saved [it will refuse to save an invalidated core], but
the exit status of the interpreter will indicate an error has occurred.

At the beginning of the tests we can only use a very limited set of words
as we have not yet tested the words the words that will allow us to provide
more useful debugging information.

Simple tests, such as checking whether we can define forth words, 
arithmetic, and control flow, are already done in other tests or are
assumed to work )

( first simple test bench, this will be redefined later )
: test 0= if invalidate-forth bye then ;

( example usage of the test bench, this will exit and invalidate
 the core if it tests )
2 2 + 4 = test

( @todo Test as many words as possible )

( ========================== Hiding Words ==================================== )

: test01 2 2 + ; test01 4 = test
find test01 0 <> test
:hide test01 ;hide
find test01 0 = test

( ========================== Hiding Words ==================================== )

( ========================== Strings ========================================= )

s" Hello, World" nip 13 = test

( ========================== Strings ========================================= )

( ========================== Better tests ==================================== )

true variable unit-color-on
0    variable unit-pass
0    variable unit-fail

: unit-colorize ( color -- : given a color, optionally colorize the unit test )
	unit-color-on @ if bright swap foreground color else drop then ;

: fail-color ( -- : set terminal color to indicate failure )
	red unit-colorize ;

: pass-color ( -- : set terminal color to indicate success )
	green unit-colorize ;

: info-color ( -- : set terminal color to for information purposes )
	yellow unit-colorize ;

: state-color
	blue unit-colorize ;

: unit-reset-color ( -- : reset color to normal )
	unit-color-on @ if reset-color then ;

: fail ( -- addr : push address to increment for failure )
	fail-color
	" FAILED: " 
	unit-fail ;

: pass ( -- addr : push address to increment for pass )
	pass-color
	" ok:     " 
	unit-pass ;

: pass-fail ( bool -- : increment pass or fail value, zero is pass )
	tab 0= 
	if fail else pass then 1+!
	unit-reset-color ;

:hide test ;hide
: test ( c-addr u -- : evaluate a string that should push a pass/fail value )
	2dup evaluate drop
	pass-fail
	type cr ;

: print-ratio ( x y -- : print x/x+y )
	2dup + nip swap u. " /" u. ;

: summary ( -- : print summary of unit tests and exit if there are problems )
	" passed: " unit-pass @ unit-fail @ print-ratio cr
	unit-fail @ if " test suite failed" cr invalidate-forth bye then ;

0 variable #current-test
256 char-table current-test
: fill-current-test 
	current-test 0 fill current-test [char] ) accepter #current-test ! ;

( @todo forget everything declared after this test )
: unit(    ( -- : Mark the beginning of a unit test )   
	state-color " unit:   " unit-reset-color fill-current-test current-test drop #current-test @ type cr ;
: end-unit ( -- : Mark the end of a unit test )
	state-color " end:    " unit-reset-color current-test drop #current-test @ type cr ;

:hide  
 unit-pass unit-fail unit-colorize fail-color pass-color info-color 
 unit-reset-color pass-fail print-ratio 
;hide

( ========================== Better tests ==================================== )

.( Starting libforth unit tests for forth code ) cr
.( 	For tests against the C API, see unit.c ) cr
.( 	For documentation about the interpreter see 'readme.md' ) cr

( ========================== Basic Words ===================================== )
( so far we have assumed that most of the basic words work anyway, but there is
no harm in adding tests here, they can always be moved around if there any problems
in terms of dependencies )

unit( Basic Words ) 

s" 1 " test 
s" 0 not " test

s" char a 97 = " test ( assumes ASCII is used )
s" bl 32 = " test ( assumes ASCII is used )
s" -1 negative? " test
s" -40494 negative? " test
s" 46960 negative? not " test
s" 0 negative? not " test

s" char / number? not " test
s" char : number? not " test
s" char 0 number? " test
s" char 3 number? " test
s" char 9 number? " test
s" char 9 number? " test
s" char x number? not " test
s" char l lowercase? " test
s" char L lowercase? not " test

s" 9 log2 3 = " test
s" 8 log2 3 = " test
s" 4 log2 2 = " test
s" 2 log2 1 = " test
s" 1 log2 0 = " test
s" 0 log2 0 = " test ( not ideal behavior - but then again, what do you expect? )

s" 50 25 gcd 25 = " test
s" 13 23 gcd 1  = " test

s" 5  5  mod 0 = " test
s" 16 15 mod 1 = " test

s" 98 4 min 4 = " test
s" 1  5 min 1 = " test
s" 55 3 max 55 = " test
s" 3 10 max 10 = " test

s" -2 negate 2 = " test
s" 0  negate 0 = " test

s" 3 2 4 within " test
s" 2 2 4 within " test
s" 4 2 4 within not " test
s" 6 1 5 limit 5 = " test
s" 0 1 5 limit 1 = " test

s" 1 2 3 3 sum 6 = " test

s" b/buf 1024 = " test ( as per the standard )

s" 1 2 3 4 5 1 pick 4 = " test

s" -1 odd 0<>" test
s" 0 odd 0=" test
s" 4 odd 0=" test
s" 3 odd 0<>" test

s" 4 square 16 = " test
s" -1 square 1 = " test

end-unit
( ========================== Basic Words ===================================== )

( ========================== Jump Tables ===================================== )

unit( Jump tables ) 
marker cleanup
: j1 1 ;
: j2 2 ;
: j3 3 ;
: j4 4 ;
create jtable find j1 , find j2 , find j3 , find j4 ,

: jump 0 3 limit jtable + @ execute ;
s" 0 jump j1 = " test
s" 1 jump j2 = " test
s" 2 jump j3 = " test
s" 3 jump j4 = " test
s" 4 jump j4 = " test ( check limit )

cleanup
end-unit

( ========================== Jump Tables ===================================== )

( ========================== Match tests ===================================== )
unit( Match )
marker cleanup

: typist ( c-addr u -- c-addr u : print out a string leaving the string on the stack )
	2dup type cr ;

.(  Match str:	) c" hello" typist drop constant matchme
.(  Pattern 1:	) c" h?ll?" typist drop constant pat1
.(  Pattern 2:	) c" h*lo"  typist drop constant pat2
.(  Pattern 3:	) c" hxllo" typist drop constant pat3
.(  Pattern 4:	) c" *"     typist drop constant pat4
.(  Pattern 5:	) c" h*llx" typist drop constant pat5

s" matchme pat1 match 1 = " test
s" matchme pat2 match 1 = " test
s" matchme pat3 match 0 = " test
s" matchme pat4 match 1 = " test
s" matchme pat5 match 0 = " test 

cleanup
end-unit
( ========================== Match tests ===================================== )

( ========================== Defer tests ===================================== )
unit( Defer/Is )
( we cannot make deferred words immediate for now, doing so would require a big
  change in the interpreter, and fixing immediate so it goes after a words
  definition... )
marker cleanup
defer alpha 

alpha constant alpha-location
: beta 2 * 3 + ;
: gamma 5 alpha ;
: delta 4 * 7 + ;
s" alpha-location gamma swap drop = " test
alpha is beta
s" gamma 13 = " test
alpha-location is delta
s" gamma 27 = " test

cleanup
end-unit
( ========================== Defer tests ===================================== )

( ========================== Move Words ====================================== )
unit( Move words )
marker cleanup
\ 128 constant len
\ len char-table t1
\ len char-table t2
\ t1 2chars erase
\ t1 type cr
( @todo implement these tests )
cleanup
end-unit
( ========================== Move Words ====================================== )


( ========================== Misc Words ====================================== )
unit( Misc words )
marker cleanup

s" 5 3 repeater 3 sum 15 = " test
s" 6 1 range mul 120 = " test

cleanup
end-unit
( ========================== Misc Words ====================================== )

summary

unit-test-framework
