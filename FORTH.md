A Small tutorial on Howe Forth.
===============================

![Howe Forth Logo](https://raw.github.com/howerj/c-forth/master/logo.png "By the power of HOWE FORTH!")

Author:             

* Richard James Howe.

Copyright:          

* Copyright 2013 Richard James Howe.

License:            

* LGPL

Email(s):              

* howe.r.j.89@googlemail.com

INTRO
=====

There are a few online tutorials dealing with Forth in various formats, but none
dealing with my specific dialect ( *and why would there be?* ). While the code
should itself is not too long and *should* be well documented there is a need
for one consistent tutorial. All the primitives are listed in *MANUAL.md*, this
tutorial just gives examples of how to use those primitives, how the environment
looks to the user and how to use some of the predefined words.

To compile the program you will need a C Compiler (any compiler *should* do so
long as it is for a hosted environment, it is written in pure ANSI C). Type
*make* and then *./forth* on a Unix like environment. You should now see the
greetings message (in color), something like:

~~~

    Howe Forth
    Base System Loaded
    @author   Richard James Howe
    ...


~~~

If this is the case then you are now at the Forth prompt and ready to type in
commands. While this program should compile under Windows with no problems, I
have not tested it and it would require GNU make to be present. There should be
nothing to stop the actual C files from being compiled that is.

RPN and Hello World!
====================

The first thing you will want when dealing with any new programming language is
either perform some basic arithmetic or print out "Hello, World!". So the first
thing we will do is add a few numbers together, type in:

~~~

2 2 + .

~~~

And you should see '4' being displayed. You will probably not be familiar with
the notation used called *Reverse Polish Notation* or *RPN*, but instead with
*infix* notation such as 2+2. It is simply a different way of doing things but
it gets the same job done, the reason it is done like this is that it makes the
implementation of the system much easier and one of the tenets of Forth is
simplicity. 

What this does is simple, it pushes two numbers onto the *variable* stack, '2'
and '2', so when it reads in a number its behavior is to push that number onto
the stack. Then it reads in a function, or a 'word' in Forth parlance and that
word is '+'. This takes two numbers off the stack and adds them, the pushes the
result. The word '.' pops a number off the stack and displays it, and this works
for other functions and numbers.

We can do the simple "Hello, World!" program either in two ways, the first we
just issue a few commands, the second we define a new function that can be
called again to do what we want type in:

~~~

    ." Hello, World!" cr

~~~

And you will see "Hello, World!" printed to the screen. To define this as a
function that can be called again:

~~~

    : hello ." Hello, World!" cr ;

~~~

And we can type "hello" to call that function. 

It is important to note that all words in Forth are space delimited and there is
nothing special about the words that handle strings. Go ahead and type:

~~~

    ."Hello, World!" cr

~~~

And you the output will look like this (but could be slightly different for
you):

~~~

    667   forth.c
    ."Hello,
    Err: Word not found.

~~~"

There is a different of a single space between

~~~

    ." Hello, World!" cr

    and

    ."Hello, World!" cr


~~~

Which makes all the difference, they are *not* the same thing, ." is simply a
word, when ." is called it will do one of two things depending on whether we are
in compile or command mode. In command mode, the first example (simply ." Hello
World!" cr) we type in commands and they are executed. In the second example we
have compile mode, where we create new functions and words. In command mode it
simply prints out the following string which is *Hello World* in this case, 'cr'
adds a newline after this. In compile mode it stores that string to be printed
out by our new function when that function is called, by typing 'hello'. 



