#!./forth 


( Forth Unit Tests
This fill contains the tests for words and functionality defined in
forth.fth. The mechanism to test whether the tests have asserted would
include looking at the core dump generated from executing this file, if
the forth interpreter has been invalidated then the tests have asserted. The
core will not be saved [it will refuse to save an invalidated core], but
the exit status of the interpreter will indicate an error has occurred.

At the beginning of the tests we can only use a very limited set of words
as we have not yet tested the words the words that will allow us to provide
more useful debugging information.

Simple tests, such as checking whether we can define forth words, 
arithmetic, and control flow, are already done in other tests or are
assumed to work )

( first simple test bench, this will be redefined later )
: assert 0= if invalidate-forth bye then ;

( example usage of the test bench, this will exit and invalidate
 the core if it asserts )
2 2 + 4 = assert

( @todo Test as many words as possible )

( ========================== Hiding Words ==================================== )

: test01 2 2 + ; test01 4 = assert
find test01 0 <> assert
:hide test01 ;hide
find test01 0 = assert

( ========================== Hiding Words ==================================== )

( ========================== Strings ========================================= )

s" Hello, World" nip 13 = assert

( ========================== Strings ========================================= )

( ========================== Better tests ==================================== )

( @todo unit( should store its string so end-unit can print it out )
( @todo if evaluate worked correctly, it could be used for better error printing )
( @todo optional color support )
:hide assert ;hide
: assert 0= if invalidate-forth " assertion failed" cr bye then ; 
: pass " Tests Completed Successfully" cr ;
: unit( " Testing Unit: " cr tab postpone .( cr ;
: end-unit " Unit Test Completed" cr ;

( ========================== Better tests ==================================== )

.( Starting libforth unit tests for forth code ) cr
.( 	For tests against the C API, see unit.c ) cr
.( 	For documentation about the interpreter see 'readme.md' ) cr

( ========================== Basic Words ===================================== )
( so far we have assumed that most of the basic words work anyway, but there is
no harm in adding tests here, they can always be moved around if there any problems
in terms of dependencies )
unit( Basic Words ) 

1 assert
0 not assert

char a 97 = assert ( assumes ASCII )
bl 32 = assert ( assumes ASCII )
-1 negative? assert
-40494 negative? assert
46960 negative? not assert
0 negative? not assert

char / number? not assert
char : number? not assert
char 0 number? assert
char 3 number? assert
char 9 number? assert
char 9 number? assert
char x number? not assert
char l lowercase? assert
char L lowercase? not assert

5  5  mod 0 = assert
16 15 mod 1 = assert

98 4 min 4 = assert
1  5 min 1 = assert
55 3 max 55 = assert
3 10 max 10 = assert

-2 negate 2 = assert
0  negate 0 = assert

3 2 4 within assert
2 2 4 within assert
4 2 4 within not assert

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
0 jump j1 = assert
1 jump j2 = assert
2 jump j3 = assert
3 jump j4 = assert
4 jump j4 = assert ( check limit )

cleanup
end-unit

( ========================== Jump Tables ===================================== )

( ========================== Move Words ====================================== )
unit( Move words )
marker cleanup
128 table t1
128 table t2

( @todo implement these tests )

cleanup
end-unit
( ========================== Move Words ====================================== )
pass
