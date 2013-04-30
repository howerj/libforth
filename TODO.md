TO DO:
======

These are some notes for myself:

* Documentation. This should be exhaustive, every enum should be documented
as the this program might be used as a library where only the header is
available. Also expand the file "MANUAL.md", which should contain the main
manual.

* Clean up code.

* Stress testing, stress testing with Valgrind (currently gives no errors).

* File stack.

* Eval()

* Cycles.

* On INI Forth should check for valid pointers, ie. Not NULL. It current does
not do this for certain pointers (input/output io)

* Remove options for changing the settings for io_stderr, it should always
point to stderr. This may pose a problem when porting to embedded devices.

* The dictionary could be implemented with a 16-bit hash instead to save
space.

* Test on more platforms, with different compilers.
