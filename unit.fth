( This file contains the unit tests for the code in "forth.fth,
when developing this should be run periodically and definitely
before checking in any code. )

( Define a new version of interpret which catches errors, but then
invalidates the Forth core and quits, signaling a test failure )
: interpret ( c1" xxx" ... cn" xxx" -- )
	begin 
	' read catch 
	?dup-if [char] ! emit tab . cr invalidate then 
	again ;

interpret ( use the new interpret word )

( We setup a marker which when called will restore the dictionary
to a state before marker is called )
marker cleanup

.( Unit tests ) cr

( By setting the trace level to one unit tests will print out the
tests they are performing )
1 trace

.( ===================== BASIC TESTS ===================== ) cr
( Most of the tests in the basic test suite a redundant, if
these words did not work then the unit test suite itself would
not work. One of the problems of making a unit test suite in
Forth is make the testing words use as few words as possible,
this unit test suite does not do so well on that front. 

Another purpose of unit tests is to document what a word should
do, so these tests do serve some utility. Not only that, but it
should be possible to rewrite T{ to use few words in the future. )


T{ 0 not -> true }T

T{ 0 1+ -> 1 }T
T{ 1 1- -> 0 }T

T{ 0 0= -> true }T
T{ 1 0= -> false }T
T{ 0 not -> true }T
T{ 1 not -> false }T
T{ 1 not -> false }T

T{ 4 logical -> true }T
T{ 1 logical -> true }
T{ -1 logical -> true }
T{ 0 logical -> false }

T{ 9 3 4 *+ -> 21 }T

T{ 4 2- -> 2 }T
T{ 6 2+ -> 8 }T

T{ 9 5 mod -> 4 }T
T{ 9 10 mod -> 9 }T

T{ 9 5 um/mod -> 4 1 }T
T{ 9 10 um/mod -> 9 0 }T

T{ 0 mask-byte -> 0xFF }T
T{ 1 mask-byte -> 0xFF00 }T

T{ 0xAA55 0 select-byte -> 0x55 }T
T{ 0xAA55 1 select-byte -> 0xAA }T

T{ -3 abs -> 3 }T
T{  3 abs -> 3 }T
T{  0 abs -> 0 }T

T{ 2 3 drup -> 2 2 }T

T{ char a -> 97 }T ( assumes ASCII is used )
T{ bl -> 32 }T ( assumes ASCII is used )
T{ -1 negative? -> true }T
T{ -40494 negative? -> true }T
T{ 46960 negative? -> false }T
T{ 0 negative? -> false }T

T{ -5 4 +- negative? -> true }T
T{  6 6 +- negative? -> false }T
T{  7 -1023 +- negative? false }T
T{  0 0 +- negative? -> false }T
T{ -1 -99 +- negative? -> true }T

T{ char / number? -> false }T
T{ char : number? -> false }T
T{ char 0 number? -> true }T
T{ char 3 number? -> true }T
T{ char 9 number? -> true }T
T{ char x number? -> false }T
T{ char l lowercase? -> true }T
T{ char L lowercase? -> false }T

T{ 9 log2 -> 3 }T
T{ 8 log2 -> 3 }T
T{ 4 log2 -> 2 }T
T{ 2 log2 -> 1 }T
T{ 1 log2 -> 0 }T

T{ 50 25 gcd -> 25 }T
T{ 13 23 gcd -> 1 }T

T{ 5  5  mod -> 0 }T
T{ 16 15 mod -> 1 }T

T{ 98 4 min -> 4 }T
T{ 1  5 min -> 1 }T
T{ 55 3 max -> 55 }T
T{ 3 10 max -> 10 }T

T{ -2 negate -> 2 }T
T{ 0  negate -> 0 }T
T{ 2  negate -> -2 }T

T{ 1 2 drup -> 1 1 }T

T{ -3 4 sum-of-squares -> 25 }T

T{ 1 2 under -> 1 1 2 }T
T{ 1 2 3 4 2nip -> 3 4 }
T{ 1 2 3 4 2over -> 1 2 3 4 1 2 }
T{ 1 2 3 4 2swap -> 3 4 1 2 }

T{ 3 ?dup -> 3 3 }T
\ T{ 0 ?dup -> }T ( need to improve T{ before this can be tested )

5 variable x
T{ 3 x +! x @ -> 8 }T
T{ x 1+! x @ -> 9 }T
T{ x 1-! x @ -> 8 }T
forget x

T{ 0xFFAA lsb -> 0xAA }T

T{ 3 2 4 within -> true }T
T{ 2 2 4 within -> true }T
T{ 4 2 4 within -> false }T
T{ 6 1 5 limit -> 5 }T
T{ 0 1 5 limit -> 1 }T

T{ 1 2 3 3 sum -> 6 }T

T{ 1 2 3 4 5 1 pick -> 1 2 3 4 5 4 }T
T{ 1 2 3 4 5 0 pick -> 1 2 3 4 5 5 }T
T{ 1 2 3 4 5 3 pick -> 1 2 3 4 5 2 }T

T{ 1 2 3 4 5 6 2rot -> 3 4 5 6 1 2 }T

T{  4 s>d ->  4  0 }T
T{ -5 s>d -> -5 -1 }T

T{ 4 5 bounds -> 9 4 }T

size 8 = [if]
T{ 0  aligned -> 0 }T
T{ 1  aligned -> size }T
T{ 7  aligned -> size }T
T{ 8  aligned -> size }T
T{ 9  aligned -> size 2* }T
T{ 10 aligned -> size 2* }T
T{ 16 aligned -> size 2* }T
T{ 17 aligned -> size 3 * }T
[then]

size 4 = [if]
T{ 0 aligned -> 0 }T
T{ 1 aligned -> size }T
T{ 3 aligned -> size }T
T{ 4 aligned -> size }T
T{ 5 aligned -> size 2* }T
T{ 8 aligned -> size 2* }T
T{ 9 aligned -> size 3 * }T
[then]

T{ 8 16 4 /string -> 12 12 }T
T{ 0 17 3 /string -> 3  14 }T

T{ -1 odd -> true }T
T{ 0 odd  -> false }T
T{ 4 odd  -> false }T
T{ 3 odd  -> true }T

T{ 4 square -> 16 }T
T{ -1 square -> 1 }T

T{ 55 signum -> 1 }T
T{ -4 signum -> -1 }T
T{ 0 signum -> 0 }T

T{ -2 3  < -> true }T
T{  2 -3 < -> false }T
T{  2  3 < -> true }T
T{ -2 -1 < -> true }T
T{ -2 -2 < -> false }T
T{  5 5  < -> false }T

T{ 5 5 <=> -> 0 }T
T{ 4 5 <=> -> 1 }T
T{ 5 3 <=> -> -1 }T
T{ -5 3 <=> -> 1  }T

( test the built in version of factorial )
T{ 6 factorial -> 720  }T
T{ 0 factorial -> 1  }T
T{ 1 factorial -> 1  }T

T{ 2 prime? -> 2 }T
T{ 4 prime? -> 0 }T
T{ 3 prime? -> 3 }T
T{ 5 prime? -> 5 }T
T{ 15 prime? -> 0 }T
T{ 17 prime? -> 17 }T

T{ 0 sqrt -> 0 }T
T{ 1 sqrt -> 1 }T
T{ 2 sqrt -> 1 }T
T{ 3 sqrt -> 1 }T
T{ 9 sqrt -> 3 }T
T{ 10 sqrt -> 3 }T
T{ 16 sqrt -> 4 }T
T{ 36 sqrt -> 6 }T

: factorial-1 dup 1 <= if drop 1 exit then 1 swap 1+ 1 do i * loop ;

T{ 0 factorial-1 -> 1 }T
T{ 1 factorial-1 -> 1 }T
T{ 2 factorial-1 -> 2 }T
T{ 3 factorial-1 -> 6 }T
T{ 4 factorial-1 -> 24 }T
T{ 5 factorial-1 -> 120 }T
T{ 6 factorial-1 -> 720 }T

: factorial-2 ( n -- n! )
	( This factorial is only here to test range, mul, do and loop )
	dup 1 <=
	if
		drop
		1
	else ( This is obviously super space inefficient )
 		dup >r 1 range r> mul
	then ;

T{ 5 3 repeater 3 sum -> 15 }T
T{ 6 1 range dup mul -> 720 }T
T{ 5 factorial-2 -> 120 }T

.( ===================== JUMP TABLES ===================== ) cr
: j1 1 ;
: j2 2 ;
: j3 3 ;
: j4 4 ;
create jtable find j1 , find j2 , find j3 , find j4 ,

: jump 0 3 limit jtable + @ execute ;
T{ 0 jump -> j1 }T
T{ 1 jump -> j2 }T
T{ 2 jump -> j3 }T
T{ 3 jump -> j4 }T
T{ 4 jump -> j4 }T ( check limit )

.( ===================== DEFER =========================== ) cr
defer alpha 
alpha constant alpha-location
: beta 2 * 3 + ;
: gamma 5 alpha ;
: delta 4 * 7 + ;
T{ alpha-location gamma swap drop = -> 1 }T
alpha is beta
T{ gamma -> 13 }T
alpha-location is delta
T{ gamma -> 27 }T

9 variable x
T{ x -1 toggle x @ -> -10 }T
T{ x -1 toggle x @ -> 9 }T
forget x

.( ===================== MATCH =========================== ) cr

T{ c" hello" drop c" hello" drop match -> true }T
T{ c" hello" drop c" hellx" drop match -> false }T
T{ c" hellx" drop c" hello" drop match -> false }T
T{ c" hello" drop c" hell"  drop match -> false }T
T{ c" hell"  drop c" hello" drop match -> false }T
T{ c" hello" drop c" he.lo" drop match -> true }T
T{ c" hello" drop c" h*"    drop match -> true }T
T{ c" hello" drop c" h*l."  drop match -> true }T

.( ===================== CRC ============================= ) cr

T{ c" xxx" crc16-ccitt -> 0xC35A }T
T{ c" hello" crc16-ccitt -> 0xD26E }T

.( ===================== RATIONALS ======================= ) cr

T{ 1 2 2 4 =rat -> true }T
T{ 3 4 5 7 =rat -> false }T

T{ 1 3 1 2 >rat -> false }T
T{ 6 20 1 5 >rat -> true }T

T{ 1 2 1 2 *rat -> 1 4 }T
T{ 8 20 100 200 *rat -> 1 5 }T

T{ 8 1 4 2 /rat -> 4 1 }T
T{ 1 2 2 4 /rat -> 1 1 }T
T{ 5 6 3 7 /rat -> 35 18 }T 
T{ 1 2 3 4 /rat -> 2 3 }T

.( ===================== NUMBER CONVERSION =============== ) cr

decimal
T{ char 0 number? -> 1 }T
T{ char 1 number? -> 1 }T
T{ char 9 number? -> 1 }T
T{ char a number? -> 0 }T
T{ char * number? -> 0 }T

hex
T{ char 8 number? -> 1 }T
T{ char a number? -> 1 }T
T{ char / number? -> 0 }T
T{ char F number? -> 1 }T

decimal
T{ 0 c" 123" >number 2drop -> 123 }T
T{ 0 c" 1"   >number 2drop -> 1   }T
T{ 0 c" 12x" >number nip   -> 12 1 }T

hex
T{ 0 c" ded" >number 2drop -> ded }T

decimal

.( ===================== COUNTED STRINGS ================= ) cr

T{ x" hello" count nip -> 5 }T




.( ===================== CONS CELLS ====================== ) cr

77 987 cons constant x
T{ x car@ -> 77  }T
T{ x cdr@ -> 987 }T
T{ 55 x cdr! x car@ x cdr@ -> 77 55 }T
T{ 44 x car! x car@ x cdr@ -> 44 55 }T

.( ===================== SKIP ============================ ) cr

T{ c" hello" char l skip nip -> 3 }T
T{ c" hello" char x skip nip -> 0 }T

cleanup

.( END OF UNIT TESTS ) cr


