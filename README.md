# SMALL FORTH

This small [FORTH][] interpreter is based on a de-obfuscated entrant
into the [IOCCC][] by *buzzard*. The entry described a [FORTH][]
like language which this derives from. You should be able to use this
interpreter as a library that you can call from your program, *main.c*
is simply a wrapper around the language. This is mainly a temporary
repository, I will integrate this project into my multicall binary
project (and other projects).

The original files are in a folder called *original/*. Simply type
**make** to get things working, this will compile the program and run
it. Type **make run** to run the interpreter (and build if it has not
been already.

[FORTH]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[IOCCC]: http://ioccc.org/winners.html
