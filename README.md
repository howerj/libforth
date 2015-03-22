# README.md

This small [FORTH][] interpreter is based on a de-obfuscated entrant
into the [IOCCC][] by *buzzard*. The entry described a [FORTH][]
like language which this derives from. You can use this library to
evaluate [FORTH][] strings or as an embeddable interpreter. Work
would need to be done to get useful information after doing those
evaluations, but the library works quite well.

*main.c* is simply a wrapper around one the functions that implements
a simple REPL.

The original files are in a self extracting archive called *third.shar*,
which includes some quite good documentation on how the interpreter works.

# To-do

* Rewrite the word header to be more compact.
* Documentation of my own.
* Dump registers on error for debugging.
* Experiment with hashing words instead of using names.

[FORTH]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[IOCCC]: http://ioccc.org/winners.html
