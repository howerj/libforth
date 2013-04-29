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

This is a small FORTH implementation called **Howe Forth**, it is written in C
with the aim of being portable, even to radically different systems from the
normal desktop to embedded systems with very few modifications.

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



Forth primitives
----------------

:
immediate
read
\\
exit
br
?br
\+
\-
\*
%
/
lshift
rshift
and
or
invert
xor
1\+
1\-
=
\<
\>
@reg
@dic
@var
@ret
@str
\!reg
\!dic
\!var
\!ret
\!str
key
emit
dup
drop
swap
over
\>r
r\>
tail
\'
,
printnum
get\_word
strlen
isnumber
strnequ
find
execute
kernel



FORTH PROGRAM
=============

REFERENCES
==========

[ioccc]: http://www.ioccc.org/winners.html "IOCCC Winner buzzard.2"

EOF
