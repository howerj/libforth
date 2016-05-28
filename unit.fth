#!./forth forth.fth

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
:hide assert ;hide
: assert 0= if invalidate-forth " assertion failed" cr bye then ; 
: pass " Tests Completed Successfully" cr ;
: unit( " Testing Unit: " postpone .( cr ;
: end-unit " Unit Test Completed" cr ;

( ========================== Better tests ==================================== )

( ========================== Basic Words ===================================== )
( so far we have assumed that most of the basic words work anyway, but there is
no harm in adding tests here, they can always be moved around if there any problems
in terms of dependencies )
unit( Testing Basic Words ) 

1 assert
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
55 3 max 55 = assert

-2 negate 2 = assert

end-unit
( ========================== Basic Words ===================================== )

pass
