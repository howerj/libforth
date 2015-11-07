# libforth.md
# A Small Forth Library.
## Introduction

[FORTH][] is an odd language that is a loyal following in certain groups, but it
is admittedly not the most practical of language as it lacks nearly everything
the modern programmer wants in a language; safety, garbage collection,
modularity and clarity. It is however possible to implement a fully working
interpreter in a few kilobytes of assembly, those few kilobytes can make for a
fairly decent programming environment giving a high utility to space used ratio.

From the [Wikipedia][] article we can neatly summarize the language:

        "Forth is an imperative stack-based computer programming language 
        and programming environment. 
        
        Language features include structured programming, reflection (the
        ability to modify the program structure during program execution), 
        concatenative programming (functions are composed with juxtaposition) 
        and extensibility (the programmer can create new commands).

        ...

        A procedural programming language without type checking, Forth features
        both interactive execution of commands (making it suitable as a shell
        for systems that lack a more formal operating system) and the ability 
        to compile sequences of commands for later execution." 

Given the nature of the [FORTH][] language it does not make for a terribly good
embeddable scripting, but it is simple to implement and can be quite fun
to use. This interpreter is based off a previous [IOCCC][] in a file called
[buzzard.2.c][], it is a descendant of that file.

Before using and understanding this library/interpreter it is useful to checkout
more literature on [FORTH][] such as [Thinking Forth][] by Leo Brodie for a
philosophy of the language, [Starting Forth][] (same Author), [Jonesforth][]
which is a specific implementation of the language in x86 assembly and
[Gforth][], a more modern and portable implementation of the language.

It is important to realize that [FORTH][] is really more a philosophy and
collection of ideas than a specific reference implementation or standard.
It has been said that an intermediate [FORTH][] user is one who has implemented
a [FORTH][] interpreter, something which cannot be said about other languages
nor is possible given their complexity. 

The saying "if you have seen one FORTH implementation, you have seen one FORTH 
implementation" applies, nearly every single [FORTH][] implementation has its 
own idea of how to go about things despite standardization efforts - in keeping 
with this library has its own idiosyncrasies (many in fact!).

## Using the interpreter

*main.c* simple calls the function *main_forth()* in *libforth.c*, this function
initializes a [FORTH][] environment and puts the user in a [REPL][] where you
can issue commands and define words. See the manual pages for list of command
line options and library calls. All commands are given using 
[Reverse Polish Notation][] (or RPN), 
        
So:

        2+(2*4)

Becomes:

        4 2 * 2 +

And brackets are no longer needed. Numbers of pushed on to the variable
stack automatically and commands (such as '\*' and '+') take their operands
off the stack and push the result. Juggling variables on the stack becomes
easier over time. To pop a value from the stack and print it there is the
'.' word.

So:

        2 2 + . 

Prints:

        4

The simplicity of the language allows for a very small interpreter, the
loop looks something like this:

        1) Read in a space delimited FORTH WORD.
        2) Is this WORD in the dictionary?
           FOUND)      Are we in IMMEDIATE mode?
                       IMMEDIATE-MODE) Execute WORD.
                                       goto 1;
                       COMPILE-MODE)   Compile WORD into the dictionary.
                                       goto 1;
           NOT-FOUND)  Is this actually a number?
                       YES) Are we in IMMEDIATE mode?
                            IMMEDIATE-MODE) Push Number onto the stack.
                                            goto 1;
                            COMPILE-MODE)   Compile a literal number.
                                            goto 1;
                       NO)  Error! Handle error
                            goto 1;

Given that we are reading in *space delimited words* if follows that the
above expression:

        2 2 + .

Would not work if we did:

        2 2+ .

Or:

        2 2 +.

As "2+" and "+." would be parsed as words, which may or may not be defined
and if they are do not have the behavior that we want. This is more apparent
when we do any kind of string handling.

## A Forth Word

The FORTH execution model uses [Threaded Code][], the layout of a word
header follows from this.

A [FORTH][] word is defined in the dictionary and has a particular format that
varies between implementations. A dictionary is simply a linked list of
[FORTH][] words, the dictionary is usually contiguous and can only grow. The
format for our [FORTH][] words is as follows:

        .-----------------------------------------------------------------.
        |        Word Header     ~                   |                    |
        .-----------------------------------------------------------------.
        | PWD | SPTR | CODE WORD | CODE WORD or DATA | DATA, optional...  |
        .-----------------------------------------------------------------.

        PWD         = A pointer to the previously declared word. If the
                      highest bit of the pointer is set this word is
                      ignored when searched for.
        SPTR        = A pointer to the name of the word in ASCII.
        CODE WORD   = A low level (virtual) machine which gets executed
                      when the word is run, if the word is a compiling word
                      this CODE WORD will be COMPILE and the next value will
                      be another CODE WORD, most like of a different type.
        CODE WORD or DATA = This will be RUN if the following DATA is a pointer
                      to the CODE WORDs of previously defined words. But it
                      could be any CODE WORD.
        DATA        = This could be anything, but it is most likely to be
                      a list of pointers to CODE WORDs of previously defined
                      words if this optional DATA field is present.

And the dictionary looks like this:

       [ Special 'fake' word ]
          .
         /|\
          |
       .----------------------------.
       | PWD | Rest of the word ... |
       .----------------------------.
          .
         /|\
          |
        ~~~~~

        ~~~~~
          |
       .----------------------------.
       | PWD | Rest of the word ... |
       .----------------------------.
          .
         /|\
          |
       .----------------------------.
       | PWD | Rest of the word ... |
       .----------------------------.
          .
         /|\
          |
       [ Previous Word Register ]

Searching of the dictionary starts from the *Previous Word Register* and ends
at a special 'fake' word.

Defining words adds them to the dictionary, we can defined words with the ':'
words like this:

        : two-times 2 * ;

Which defined the word "two-times", a word that takes a value from the stack,
multiplies it by two and pushes the results back onto the stack.

The word ':' does several things, it is an immediate word that reads in the
next space delimited word from the input stream and creates a header for that
word. It also switches the interpreter into compile mode, compiling words will
be compiled into that word definition instead of being executed, immediate words
are executed as normal. ';' is also an immediate word, it compiles a special
word exit into the dictionary which returns from a word call and switches the
interpreter back into command mode. This type of behavior is very typical of
[FORTH][] implementations.

## Memory Map and Special Registers

The way this interpreter works is that is emulates an idealized machine, one
built for executing [FORTH][] directly. As such it has to make compromises and
treats certain sectors of memory as being special, as shown below (numbers are
given in *hexadecimal* and are multiples of the virtual machines word-size
which is a *uint16_t*):

        .----------------------------------------------------------------.
        | 0-1F      | 20-3FFF       | 4000-7BFF      |7C00-7DFF|7E00-7FFF|
        .----------------------------------------------------------------.
        | Registers | Dictionary... | Word names ... | V stack | R stack |
        .----------------------------------------------------------------.

        V stack = The Variable Stack
        R stack = The Return Stack

Each may be further divided into special sections:

### Registers

* 0x0:      The dictionary pointer
* 0x1:      The return stack pointer
* 0x2-0x7:  A 'fake' word used to implement literal numbers
* 0x8:      State, the state of the interpreter, either in *compile* or *command* mode.
* 0x9:      Hex, if none zero numbers get output in hexadecimal, otherwise they are output in decimal.
* 0xA:      Pointer to the previously declared word in the dictionary.
* 0xB:      Pointer into the string storage, this is a character pointer not a word pointer.
* 0xC-0x1F: Reserved register locations.

### Dictionary

Apart from the constraints that the dictionary begins after where the 
registers are and before where string storage is there are no set demarcations
for each region, although currently the defined word region ends before
0x200 leaving room between that and 0x3FFF for user defined words.

        .----------------------------------------------------------------.
        | 20-???            | ???-???          | ???-3FFF                |
        .----------------------------------------------------------------.
        | Special read word | Interpreter word | Defined word ...        |
        .----------------------------------------------------------------.

        Special read word = A word called on entrance to the interpreter, 
                            it calls itself recursively (as a tail call). This
                            word cannot be 'found', it does not have a name.
        Interpreter word  = Any named (not 'invisible' ones) interpreter word 
                            gets put here.
        Defined word      = A list of words that have been defined with ':'

### Word names

The storage area for the names of the words is disjoint from the rest of
the word definition, this is for simplicity of the implementation which
might be corrected in future versions.

The first 32 bytes are reserved for the storage of the current word that
is being read in, only 31 characters are allowed in a word name as the 32nd
character is reserved for the NUL character. 

The rest of the dictionary contains NUL separated strings that words point
into.

        .----------------------------------------------------------------.
        | 4000-401F              | 4020-7BFF                             |
        .----------------------------------------------------------------.
        | Currently read in word | List of null terminated defined words |
        .----------------------------------------------------------------.

## Glossary of FORTH words

Each word is also given with its effect on the variable stack, any other effects
are documented (including the effects on other stacks). Each entry looks like
this:

* word ( y -- z )

Where 'word' is the word being described, the contents between the parenthesis
describe the stack effects, this word expects one number to be one the stack,
'y', and returns a number to the stack 'z'.

### Internal words

There are three types of words.

#### 'Invisible' words

These invisible words have no name but are used to implement the FORTH. They
are all *immediate* words.

* push ( -- x)

Push the next value in the instruction stream onto the variable stack, advancing
the instruction stream.

* compile ( -- )

Compile a pointer to the next instruction stream value into the dictionary.

* run ( -- )

Save the current instruction stream pointer onto the return stack and set
the pointer instruction stream pointer to point to value after *run*.

* The special read word ( -- )

#### Immediate words

These words are named and are *immediate* words.

* ':'           ( -- )

Read in a new word from the input stream and compile it into the dictionary.

* 'immediate'   ( -- )

Make the previously declared word immediate. Unlike in most FORTH
implementations this is used after the words name is given not after the
final ';' has been reached.

So:

        : word immediate ... ;

Instead of:

        : word ... ; immediate

* '\\'           ( -- )

A comment, ignore everything until the end of the line.

#### Compiling words

* 'read'        ( -- )

*read* is a complex word that implements most of the user facing interpreter,
it reads in a [FORTH][] *word* (up to 31 characters), if this *word* is in
the *dictionary* it will either execute the word if we are in *command mode*
or compile a pointer to the executable section of the word if in *compile
mode*. If this *word* is not in the *dictionary* it is checked if it is a
number, if it is then in *command mode* we push this value onto the *variable
stack*, if in *compile mode* then we compile a *literal* into the *dictionary*.
If it is none of these we print an error message and attempt to read in a
new word.

* '@'           ( address -- x )

Pop an address and push the value at that address onto the stack.

* '!'           ( x address -- )

Given an address and a value, store that value at that address.

* '-'           ( x y -- z )

Pop two values, subtract 'y' from 'x' and push the result onto the stack.

* '+'           ( x y -- z )

Pop two values, add 'y' to 'x' and push the result onto the stack.

* '&'           ( x y -- z )

Pop two values, compute the bitwise 'AND' of them and push the result on to
the stack.

* '|'           ( x y -- z )

Pop two values, compute the bitwise 'OR' of them and push the result on to
the stack.

* '^'           ( x y -- z )

Pop two values, compute the bitwise 'XOR' of them and push the result on to
the stack.

* '~'           ( x y -- z )

Perform a bitwise negation on the top of the stack.

* '\*'          ( x y -- z )

Pop two values, multiply them and push the result onto the stack.

* '/'           ( x y -- z )

Pop two values, divide 'x' by 'y' and push the result onto the stack.

* '\<'          ( x y -- z )

Pop two values, compare them (y < x) and push the result onto the stack.

* 'exit'        ( -- )

Pop the return stack and set the instruction stream pointer to that
value.

* 'emit'        ( char -- )

Pop a value and emit the character to the output.

* 'key'         ( -- char )

Get a value from the input and put it onto the stack.

* 'r>'          ( -- x )
        
Pop a value from the return stack and push it to the variable stack.

* '>r'          ( x -- )

Pop a value from the variable stack and push it to the return stack.

* 'j'           ( -- )

Jump unconditionally to the destination next in the instruction stream.

* 'jz'          ( bool -- )

Pop a value from the variable stack, if it is zero the jump to the
destination next in the instruction stream, otherwise skip over it.

* '.'           ( x -- )

Pop a value from the variable stack and print it to the output either
as a ASCII decimal or hexadecimal value depending on the HEX register.

* '''           ( -- )

Push the next value in the instruction stream onto the variable stack
and advance the instruction stream pointer over it.

* ','           ( x -- )

Write a value into the dictionary, advancing the dictionary pointer.

* '='           ( x y -- z )

Pop two values, perform a test for equality and push the result.

* 'swap'        ( x y -- y z )

Swap two values on the stack.

* 'dup'         ( x -- x x )

Duplicate a value on the stack.

* 'drop'        ( x -- )

Drop a value.

* 'over'        ( x y -- x y x )

Duplicate the value that is next on the stack.

* 'tail'        ( -- )

A drop but for the return stack, it is used in lieu of a *recurse* word
used in most FORTH implementations.

* 'save'        ( address block-number -- )

Given an address, attempt to write out the values addr to addr+1023 values
out to disk, the name of the block will be 'XXXX.blk' where the 'XXXX' is
replaced by the hexadecimal representation of *blocknum*.

* 'load'        ( address block-number -- )

Like *save*, but attempts to load a block of 1024 words into an address in
memory of a likewise *blocknum* derived name as in *save*.

* 'find'        ( -- execution-token )

Find a word in the dictionary pushing a pointer to that word onto the
variable stack.

* '.s'

Print out the contents of the variable stack (and the top of the stack) but
do not affect the contents.

* 'print'       ( char-address -- )

This prints a NUL terminate string at *charptr*. *charptr* is a character
aligned pointer not a machine-word aligned pointer.

### Defined words

Defined words are ones which have been created with the ':' word, some words
get defined before the user has a chance to define their own to make their
life easier.

* 'state'       ( bool -- )

Change the interpreter state, turning the mode from 'compile' to 'immediate'.

* ';'           ( -- )

Write 'exit' into the dictionary and switch back into command mode.

* 'hex'         ( bool -- )

Change the state of the output of number printing, 0 for decimal which is the
default, anything else and it prints hexadecimal.

* 'pwd'         ( -- pointer )

Pushes a pointer to the previously define word onto the stack.

* 'h'           ( -- pointer )

Push a pointer to the dictionary pointer register.

* 'r'           ( -- pointer )

Push a pointer to the register pointer register.

* 'here'        ( -- dictionary-pointer )

Push the current dictionary pointer (equivalent to "h @").

* '\['          ( -- )

Immediately switch into command mode.

* ':noname'     ( -- execution-token )

This creates a word header for a word without a name and switches to compile
mode, the usual ';' finishes the definition. It pushes a execution token onto
the stack that can be written into the dictionary and run, or executed directly.

* 'if'          ( bool -- )

Begin an if-else-then statement. If the top of stack is true then we
execute all between the if and a corresponding 'else' or 'then', otherwise
we skip over it.

Abstract Examples:

        : word ... bool if do-stuff ... else do-other-stuff ... then ... ;

        : word ... bool if do-stuff ... then ... ;

and a concrete examples:

        : test-word if 2 2 + . cr else 3 3 * . cr ;
        0 test-word
        4             # prints 4
        1 test-word
        9             # prints 9

Is a simple and contrived example.


* 'else'        ( -- )

See 'if'.

* 'then'        ( -- )

See 'if'.

* 'begin'       ( -- )

This marks the beginning of a loop.

* 'until'       ( bool -- )

Loop back to the corresponding 'begin' if the top of the stack is zero, continue
on otherwise.

* '0='          ( x -- bool )

Is the top of the stack zero?

* "')'"         ( -- char )

Push the number representing the ')' character onto the stack.

* '1+'          ( x -- x )

Add one to the top of the stack.

* '1-'          ( x -- x )

Subtract one from the top of the stack.

* 'tab'         ( -- )

Print a tab.

* 'cr'          ( -- )

Prints a newline.

* '.('          ( -- )

Print out the characters in the input stream, until a ')' is encountered.

        .( Hello, World)

Will print:

        Hello, World

* '2dup'        ( x y -- x y x y )

Duplicate two items on the stack.

* 'line'        ( address -- address )

Given an address print out the address and the contents of the four
consecutive addresses and push the original address plus four. This is
a helper word for 'list'.

* 'list'        ( address-1 address-2 -- )

Given two memory address, address-2 being the larger address, print out
the contents of memory between those two addresses.

* 'allot'       ( amount -- )

Allocate a number of cells in the dictionary.

* 'words'       ( -- )

Print out a list of all the words in the dictionary that are reachable.

* 'tuck'        ( x y -- y x y )

The stack comment documents this word completely.

* 'nip'         ( x y -- y )

The stack comment documents this word completely.

* 'rot'         ( x y z -- z x y )

The stack comment documents this word completely.

* '-rot'        ( x y z -- y z x )

The stack comment documents this word completely.

* '?'           ( bool -- )

This implements conditional execution, if true then the rest of the
current input line is not executed, for example

        0 ? 2 2 + . cr

Does nothing.

        1 ? 2 2 + . cr

Prints '4'.

* '::'          ( -- )

Unlike ':' this is a compiling word, but performs the same function.

* 'create'      ( -- address )

## Glossary of FORTH terminology 

* Word vs Machine-Word

Usually in computing a 'word' refers to the natural length of integer in a
machine, I specifically use the term 'Machine-Word' when I want to invoke
this meaning, a word in [FORTH][] is more analogous to a function, but there
are different types of FORTH words; *immediate* and *compiling* words,
*internal* and *defined* words and finally *visible* and *invisible* words.

Suffice to say the distinction between a machine word and a FORTH word
can lead to some confusion.

* *The* dictionary

There is only one dictionary in a normal [FORTH][] implementation, it is a
data structure that can only grow in size (or at least it can in this
implementation) and holds all of the defined words.

* *The* stack

When we referring to a stack, or the stack, we refer to the variable stack
unless otherwise stated (such as the return stack). The variable, or the
stack, holds the result of recent operations such as addition or subtraction.

* The return stack

Forth implementations are two (or more) stack machines. The second stack
is the return stack which holds the usual function call return values as 
well as temporary variables. 

* Defined Words

A defined word is one that is not implement directly by the interpreter but
has been create with the ':' word. It can be an *immediate* word, but does
not have to be.

* Compile mode

In this mode we *compile* words unless those words are *immediate* words,
if the are then we immediately execute them.

* Command mode

In this mode, regardless of whether we are in *command* or *compile* mode
we execute words or push them on to the stack.

* A block.

A [FORTH][] block is primitive way of managing persistent storage and this
version of block interface is more primitive than most. A block is a
contiguous range of bytes, usually 1024 of them as in this instance, and
they can be written or read from disk. Flushing of dirty blocks is not
performed in this implementation and must be done 'manually'.

## To-do and notes

This is more like a list of wants or likes as well as notes. It is not
meant to be particularly coherent.

* Add a better *create* and a *does>* .
  * This would allow for 'variable', 'array' and 'constant' to be defined
  as well as perhaps some string handling functions.
* 'recurse' keyword. Although I do not really use it.
* Hide the currently defined word until it has been completely defined?
  This would necessitate there being a 'recurse' keyword.
* Immediate words should be defined by a flag not having two code words
and trickery.
* Figure out if there are any more primitives that need adding that would
  aide in scripting, perhaps file handles, floating point values, larger
  VM sizes.
* A stack based interface to the FORTH environment would be good as well,
so we can use this more as a library we can shell work out to.
  - A function for adding in user defined code words would be good. It
  would not have to be too complex.
  * forth\_define("name", func\_ptr)
  * forth\_get\_top\_of\_stk();
  * ...
* A small example that is integrated with the linenoise library would be
good, with auto complete! A list of all defined words would need to be
exported from the library however.
* Hex input needs to be handled correctly.
* 32-bit version? Allow for power of 2 size of environment; 2048, 4096, ...
* Redesign *FORTH* word header.
* Possible redesigns of FORTH word pointer to included more modes apart
from just being hidden. I could use some trickery to accomplish this
and free up more bits. I could store some words as just hashes, or some
entire words in the lower bits (for example single or two character words,
the latter requiring encoding the remaining bits). If the string was stored
*relative* to the current word then I would have even more spare bits to
mess around with.

[FORTH]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[Wikipedia]: https://en.wikipedia.org/wiki/Forth_%28programming_language%29
[IOCCC]: http://ioccc.org/winners.html
[buzzard.2.c]: http://www.ioccc.org/1992/buzzard.2.c
[REPL]: https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop
[Thinking Forth]: http://thinking-forth.sourceforge.net/
[Starting Forth]: http://www.forth.com/starting-forth/
[Jonesforth]: https://rwmj.wordpress.com/2010/08/07/jonesforth-git-repository/
[Gforth]: https://www.gnu.org/software/gforth/
[Reverse Polish Notation]: https://en.wikipedia.org/wiki/Reverse_Polish_notation
[Threaded Code]: https://en.wikipedia.org/wiki/Threaded_code

<style type="text/css">body{margin:40px auto;max-width:650px;line-height:1.6;font-size:18px;color:#444;padding:0 10px}h1,h2,h3{line-height:1.2}</style>
