#!./forth
\ A simple hexdump utility
: char key drop key ;
: literal 2 , , ;
: ':' [ char : literal ] ;
: source-id-reg 11 ;
: hexdump
	1 hex
	1 source-id-reg ! \ read from stdin
	0
	begin
		dup dup
		3 and if drop else  cr . tab ':' emit tab then
		1+
		key dup 
		-1 = if drop 1 else . tab 0 then
	until
	cr
	drop
	2 source-id-reg !
;

hexdump 
