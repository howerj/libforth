#!./forth 

( ==================== Unit tests ============================ )
1 trace !

marker cleanup
T{ 0 not -> 1 }T

T{ 0 1+ -> 1 }T
T{ 1 1- -> 0 }T

T{ 0 0= -> 1 }T
T{ 1 0= -> 0 }T
T{ 0 not -> 1 }T
T{ 1 not -> 0 }T
T{ 1 not -> 0 }T

T{ 4 logical -> 1 }T
T{ 1 logical -> 1 }
T{ -1 logical -> 1 }
T{ 0 logical -> 0 }

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

T{ char a -> 97 }T ( assumes ASCII is used )
T{ bl -> 32 }T ( assumes ASCII is used )
T{ -1 negative? -> 1 }T
T{ -40494 negative? -> 1 }T
T{ 46960 negative? -> 0 }T
T{ 0 negative? -> 0 }T

T{ char / number? -> 0 }T
T{ char : number? -> 0 }T
T{ char 0 number? -> 1 }T
T{ char 3 number? -> 1 }T
T{ char 9 number? -> 1 }T
T{ char x number? -> 0 }T
T{ char l lowercase? -> 1 }T
T{ char L lowercase? -> 0 }T

T{ 9 log2 -> 3 }T
T{ 8 log2 -> 3 }T
T{ 4 log2 -> 2 }T
T{ 2 log2 -> 1 }T
T{ 1 log2 -> 0 }T
T{ 0 log2 -> 0 }T ( not ideal behavior - but then again, what do you expect? )

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

5 variable x
T{ 3 x +! x @ -> 8 }T
T{ x 1+! x @ -> 9 }T
T{ x 1-! x @ -> 8 }T
forget x

T{ 0xFFAA lsb -> 0xAA }T

T{ 3 ?dup -> 3 3 }T
\ T{ 0 ?dup -> }T ( need to improve T{ before this can be tested )

T{ 3 2 4 within -> 1 }T
T{ 2 2 4 within -> 1 }T
T{ 4 2 4 within -> 0 }T
T{ 6 1 5 limit -> 5 }T
T{ 0 1 5 limit -> 1 }T

T{ 1 2 3 3 sum -> 6 }T

T{ 1 2 3 4 5 1 pick -> 1 2 3 4 5 4 }
T{ 1 2 3 4 5 0 pick -> 1 2 3 4 5 5 }
T{ 1 2 3 4 5 3 pick -> 1 2 3 4 5 2 }

T{ 1 2 3 4 5 6 2rot -> 3 4 5 6 1 2 }

T{  4 s>d ->  4  0 }T
T{ -5 s>d -> -5 -1 }T

T{ -1 odd -> 1 }T
T{ 0 odd  -> 0 }T
T{ 4 odd  -> 0 }T
T{ 3 odd  -> 1 }T

T{ 4 square -> 16 }T
T{ -1 square -> 1 }T

T{ 55 signum -> 1 }T
T{ -4 signum -> -1 }T
T{ 0 signum -> 0 }T

T{ 5 5 <=> -> 0 }T
T{ 4 5 <=> -> 1 }T
T{ 5 3 <=> -> -1 }T
T{ -5 3 <=> -> 1  }T

T{ -2 3  < -> 1 }T
T{  2 -3 < -> 0 }T
T{  2  3 < -> 1 }T
T{ -2 -1 < -> 1 }T
T{ -2 -2 < -> 0 }T
T{  5 5  < -> 0 }T

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

: factorial ( n -- n! )
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
T{ 5 factorial -> 120 }T

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

T{ 1 2 under -> 1 1 2 }T
T{ 1 2 3 4 2nip -> 3 4 }
T{ 1 2 3 4 2over -> 1 2 3 4 1 2 }
T{ 1 2 3 4 2swap -> 3 4 1 2 }

cleanup

( ==================== Unit tests ============================ )


