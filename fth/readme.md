# readme.md

This directory contains Forth programs that do not belong in "forth.fth", they
may have different licenses from the main program (as they are from elsewhere).
They all require at minimum the "forth.fth" file to be executed before they are
run.

For example:

	make
	./forth -t forth.fth fth/<PROGRAM-NAME>.fth

## Current Programs

* bf.fth

This program implements a Brain F\*ck compiler.

* bnf.md

This is currently not a working program, it describes a BNF parser implemented
as a series of Forth words. This program would need to be translated before
it could be used with libforth - but it looks to be a very useful program.

<style type="text/css">body{margin:40px auto;max-width:850px;line-height:1.6;font-size:16px;color:#444;padding:0 10px}h1,h2,h3{line-height:1.2}</style>
