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

This is a portable FORTH implementation called "Howe Forth", it is designed to
be customizable to your needs, modular and fairly fast. Eventually it is
intended that this will be ported to an embedded system (Most likely ARM based),
for now it will stay on the computer.

TODO:
=====

* Documentation. This should be exhaustive, every enum should be documented
as the this program might be used as a library where only the header is
available. Also expand the file "MANUAL.md", which should contain the main
manual.

* Clean up code.

* Stress testing, stress testing with Valgrind (currently gives no errors).

* File stack.

* Eval()

* Cycles.

NOTES
=====

The dictionary could be implemented with a 16-bit hash instead to save
space.

EOF
