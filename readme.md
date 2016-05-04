# README.md

This small [Forth][] interpreter is based on a de-obfuscated entrant
into the [IOCCC][] by *buzzard*. The entry described a [Forth][]
like language which this derives from. You can use this library to
evaluate [Forth][] strings or as an embeddable interpreter. Work
would need to be done to get useful information after doing those
evaluations, but the library works quite well.

*main.c* is simply a wrapper around one the functions that implements
a simple REPL.

The original files are in a self extracting archive called *third.shar*,
which includes some quite good documentation on how the interpreter works.

This project implements a [Forth][] interpreter library which can be embedded
in other projects, it is incredibly minimalistic, but usable. To build the
project a [C][] compiler is needed, and a copy of [Make][], type:

	make help

For a list of build options. 

Documentation can be found in [liblisp.md][].

[Forth]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[IOCCC]: http://ioccc.org/winners.html
[Make]: https://en.wikipedia.org/wiki/Make_%28software%29
[C]: https://en.wikipedia.org/wiki/C_%28programming_language%29
[liblisp.md]: liblisp.md
