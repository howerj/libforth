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

# Introduction

This project implements a [FORTH][] interpreter library which can be embedded
in other projects, it is incredibly minimalistic, but usable.

# To-do

* Rewrite the word header to be more compact.
* Dump registers on error for debugging.
* 32/64-bit version instead of 16-bit, and with variable length allocation
  on init.
* Integrate "create/does>" into the start up code, including
  "constant/array/variable".
* A usable API should be made.
  * Improve the unit tests after API improvements
* Add more useful assertions throughout the code, checking ranges and
  the internal logic of the system makes sense.
  * Some error handling can be replaced with assertions, which might
    be unforgiving, but would mean the interpreter would always be in
    a good state.
* Experiment with different names for [FORTH][] words.
  * Perhaps the more traditional upper case words could be used.
  * Shorter words could be used such as:
        s       swap
        d       dup
        D       drop
        h       here
        t       tuck
        n       nip
        o       over
        {       begin
        }       until
        r       rot
        -r      -rot
        2d      2dup
        2D      2drop

## Would be nice to have

* Better string handling (would be nice).
* It would be possible to make a tool for analyzing FORTH core dumps 
  produced by this tool.
* A large library of example code could be produced.

* Potential space saving measures
  * The code word name could be stored with the code word instead
    of using a pointer to it in the code word.
  * Instead using a pointer to a code word and a code word name, the
    top bit could be used to indicate that the code word name field
    is either a pointer (if not set), or a literal (if set). Alternatively
    it could mean it is a hash (if not set, collisions would have to
    be manually avoided), or a literal (if set). If a literal is used
    that would mean a minimum of two ASCII characters could be used for the
    name.
  * Bits could be used instead of using a code to indicate whether
    a word is executable or not.

[FORTH]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[IOCCC]: http://ioccc.org/winners.html
