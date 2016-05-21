#!./forth forth.fth

( Forth Unit Tests
This fill contains the tests for words and functionality defined in
forth.fth. The mechanism to test whether the tests have failed would
include looking at the core dump generated from executing this file, if
the forth interpreter has been invalidated then the tests have failed. The
core will not be saved [it will refuse to save an invalidated core], but
the exit status of the interpreter will indicate an error has occurred.

At the beginning of the tests we can only use a very limited set of words
as we have not yet tested the words the words that will allow us to provide
more useful debugging information.

Simple tests, such as checking whether we can define forth words, 
arithmetic, and control flow, are already done in other tests or are
assumed to work )

( first simple test bench, this will be redefined later )
: invalidate-forth 1 24 ! ;
: fail 0= if invalidate-forth bye then ;

( example usage of the test bench, this will exit and invalidate
 the core if it fails )
2 2 + 4 = fail

( @todo Test as many words as possible )

.( Tests Completed Successfully ) cr

