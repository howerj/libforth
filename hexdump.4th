#!./forth
( A simple hexdump utility )
: char key drop key ;
: literal 2 , , ;
: ':' [ char : literal ] ;
: file-input-reg 16 ;
: stdin-reg 18 ;
: eof -1 ;
: hexdump
	16 base !
	stdin-reg @ file-input-reg !
	0
	begin
		dup dup
		3 and if drop else  cr . tab ':' emit tab then
		1+
		key dup 
		eof = if drop 1 else . tab 0 then
	until
	cr
	drop
	0 r !
;

hexdump 

