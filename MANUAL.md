A Portable FORTH implementation: Howe Forth
===========================================

Author:             

* Richard James Howe.

Copyright:          

* Copyright 2013 Richard James Howe.

License:            

* LGPL

Email(s):              

* howe.r.j.89@googlemail.com

* howerj@aston.ac.uk

INTRO
=====

This is a small [FORTH][forthwiki] implementation called **Howe Forth**,
it is written in C with the aim of being portable, even to radically
different systems from the normal desktop to embedded systems with very
few modifications.

This interpreter traces its lineage back to an entry from the [ioccc][ioccc]. A
person submitting under the name **buzzard** entered two winning entries, one
listed as *buzzard.2* in the year 1992 was a small FORTH interpreter, this one
is a relative of it.

The interpreter should execute fairly quickly, more in due to its small size as
opposed and sparse feature set. I have not performed any benchmarks however.

As I am working and updating the interpreter this documentation is going to lag
behind that and may go out of date, although it should not do so horrendously.

C PROGRAM
=========

GCC Options
-----------

The program is compiled with the following flags:

"-ansi -g -Wall -Wno-write-strings -Wshadow -Wextra -pedantic -O2"

In [GCC][GCC].

There are some extra options that can be added on, both of which are simply
defining macros.

"-DRUN4X"

This definition enables a cycle counter, the forth virtual machine will exit
when the counter is less than zero, it is automatically decrement each time a
primitive is run. It does not denote real time spent as each primitive has a
variable run time. The forth programming running can enable or disable this
counter, as well as update it.

"-DEBUG_PRN"

This definition enables a function the runs after the virtual machine has
exited. It simply writes out all of the memory in hexadecimal encoded ASCII with
a little bit of formatting to a file called "memory.txt".

forth.c
-------

This file implements the forth virtual machine, it is a threaded code
interpreter. It provides mechanisms for error handling and recovery, an
interface for system calls and input and output redirection.

forth.h
-------

Contained in this file are the usual things needed for interfacing with a
library; definitions of structures, #define macros, typedefs, function
prototypes and the like. There is no executable code in here.

To use this code in your program you must first include "stdio\.h" then include
"forth\.h". "main\.c" contains an example of how to initialize the interpreter
memory, I will give a short example of what to do with reference to the code:

~~~

    fobj_t *forth_obj_create(mw reg_l, mw dic_l, mw var_l, mw ret_l, mw str_l)
    {
            /*the vm forth object */
            fobj_t *fo = calloc(1, sizeof(fobj_t));

            /*setting i/o streams*/
            fo->in_file = calloc(1, sizeof(fio_t));
            fo->out_file = calloc(1, sizeof(fio_t));
            fo->err_file = calloc(1, sizeof(fio_t));

            fo->in_file->fio = io_stdin;
            fo->out_file->fio = io_stdout;
            fo->err_file->fio = io_stderr;

            /*memories of the interpreter */
            fo->reg = calloc(reg_l, sizeof(mw));
            fo->dic = calloc(dic_l, sizeof(mw));
            fo->var = calloc(var_l, sizeof(mw));
            fo->ret = calloc(ret_l, sizeof(mw));
            fo->str = calloc(str_l, sizeof(char));

            /*initialize input file, fclose is handled elsewhere */
            fo->in_file->fio = io_rd_file;
            if ((fo->in_file->iou.f = fopen("forth.fs", "r")) == NULL) {
                    fprintf(stderr, "Unable to open initial input file!\n");
                    return NULL;
            }

            /*initializing memory */
            fo->reg[ENUM_maxReg] = MAX_REG;
            fo->reg[ENUM_maxDic] = MAX_DIC;
            fo->reg[ENUM_maxVar] = MAX_VAR;
            fo->reg[ENUM_maxRet] = MAX_RET;
            fo->reg[ENUM_maxStr] = MAX_STR;
            fo->reg[ENUM_inputBufLen] = 32;
            fo->reg[ENUM_dictionaryOffset] = 4;
            fo->reg[ENUM_sizeOfMW] = sizeof(mw);
            fo->reg[ENUM_INI] = true;
            fo->reg[ENUM_cycles] = false; 
            fo->reg[ENUM_ccount] = 0; 

            fprintf(stderr, "\tOBJECT INITIALIZED.\n");
            return fo;
    }

~~~

For brevities sake checking whether "calloc()" worked it omitted. This function
"forth\_obj\_create()" takes in a list of lengths for each of the arrays, which
will be checked by the interpreter when called to see if they meet a list of
minimum requirements, and allocates memory for them, that much is obvious. 

Of interest however is what "reg[X]" is being set to and why and what do
"in\_file", "out\_file" and "err\_file" do. The latter are objects which contain
descriptions of where the input, output and the error streams are to be sent to.
You can read or write to "stdin", "stdout" and "stderr" (if possible), to
strings or to file streams.

Each "reg[X]" value is going to require explaining.

~~~

All MAX_X Assignments tell the forth virtual machine what the
maximum offset is for each array passed to it.

    fo->reg[ENUM_maxReg] = MAX_REG;
    fo->reg[ENUM_maxDic] = MAX_DIC;
    fo->reg[ENUM_maxVar] = MAX_VAR;
    fo->reg[ENUM_maxRet] = MAX_RET;
    fo->reg[ENUM_maxStr] = MAX_STR;

"inputBufLen" is the length of the input buffer, it limits how big
a defined word can be as well as some other things.

    fo->reg[ENUM_inputBufLen] = 32;

The first four cells in the dictionary should be zero, this is the offset into
that dictionary. The reason for this is that it contains a 'fake' dictionary
word which pushes zero if accidentally run, points to itself, points to the
currently processed word in string storage and will run the first virtual
machine instruction when called 'push', although this information is not
necessary for the anything, just set it to '4'.

    fo->reg[ENUM_dictionaryOffset] = 4;

Simply the size of the forth virtual machines machine word. It should be at
least two bytes big, preferably signed (if unsigned many warnings will be
present, but apart from that there should be no problems).

    fo->reg[ENUM_sizeOfMW] = sizeof(mw);

This sets a flag to be true, when the "INI" flag is true the first time the
function "forth\_interpreter()" is run it will attempt to set up the forth
environment to an initial state (by calling "forth\_initialize()"). This checks
whether the minimum memory requirements are met, sets up some other registers to
their initial values, it also gets a list of symbols to be used the forth
virtual machine - that is you can change their name to what you want. It finally
creates a forth function that calls a primitive called 'read' which then calls
itself ('read' makes sure the return stack does not blow up). 

    fo->reg[ENUM_INI] = true;

When compiled with the flag "-DRUN4X" each time a primitive is run and 'cycles'
is enabled a counter is decrement when this is less than zero
"forth\_interpreter()" will exit. This makes sure it starts out as false.

    fo->reg[ENUM_cycles] = false; 
    fo->reg[ENUM_ccount] = 0; 

~~~

main.c
------

This file simply contains a way to setup the library; it allocates all the
needed memory, sets up the initial input file stream (a file called **forth.fs**)
and finally sets up some sane starting variables for the allocated memory.

Having done this it then runs the interpreter, when it exits it will display the
functions return value and then destroys the object given to it. The interpreter
itself will close the initial input file **forth.fs** when it reaches a EOF
character.

Forth primitives
----------------

* ":":
* "immediate":
* "read":
* "\\":
* "exit":
* "br":
* "?br":
* "\+":
* "\-":
* "\*":
* "%":
* "/":
* "lshift":
* "rshift":
* "and":
* "or":
* "invert":
* "xor":
* "1\+":
* "1\-":
* "=":
* "<":
* "\>":
* "@reg":
* "@dic":
* "@var":
* "@ret":
* "@str":
* "\!reg":
* "\!dic":
* "\!var":
* "\!ret":
* "\!str":
* "key":
* "emit":
* "dup":
* "drop":
* "swap":
* "over":
* "\>r":
* "r\>":
* "tail":
* "\'":
* ",":
* "printnum":
* "get\_word":
* "strlen":
* "isnumber":
* "strnequ":
* "find":
* "execute":
* "kernel":

In addition to this there are three 'invisible' forth words which do not have
a name which are:

* "push integer":
* "compile":
* "run":

You can that being limited to only these primitives would not create a very
forth-like system. However as any forth programmer knows you can extend the
language in itself, which is what the first file does that is read in.

FORTH PROGRAM
=============


FINAL WORDS
===========

This program is released under the [LGPL][LGPL], feel free to use it in your
project under LGPLs restrictions. Please contact me if you have any problems
at my listed Email: [howe.r.j.89@gmail.com][EMAIL]. 

<!-- REFERENCES -->

[ioccc]: http://www.ioccc.org/winners.html "IOCCC Winner buzzard.2"
[forthwiki]: https://en.wikipedia.org/wiki/FORTH "FORTH (programming language)"
[LGPL]: http://www.gnu.org/licenses/lgpl-3.0.txt "LGPL License"
[EMAIL]: mailto:howe.r.j.89@gmail.com "howe.r.j.89@gmail.com"
[GCC]: http://gcc.gnu.org/ "The GNU C Compiler"
EOF
