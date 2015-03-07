# small forth library 

This small [FORTH][] interpreter is based on a de-obfuscated entrant
into the [IOCCC][] by *buzzard*. The entry described a [FORTH][]
like language which this derives from. You should be able to use this
interpreter as a library that you can call from your program, *main.c*
is simply a wrapper around the language. This is mainly a temporary
repository, I will integrate this project into my multicall binary
project (and other projects).

The original files are in a self extracting archive called *third.shar*,
which includes some quite good documentation on how the interpreter works.

# To-do

* Rewrite the word header to be more compact.
* Dump registers on error for debugging.
* Experiment with hashing words instead of using names.

[FORTH]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[IOCCC]: http://ioccc.org/winners.html
