#!./forth
( A simple hexdump utility : work in progress, no prerequisites )
: char key drop key ;
: literal 2 , , ;
: ':' [ char : literal ] ;
: eof -1 ;
: hexdump
	16 base !
	`stdin @ `fin !
	0
	begin
		dup dup
		3 and if drop else  cr pnum tab ':' emit tab then
		1+
		key dup 
		eof = if drop 1 else pnum tab 0 then
	until
	cr
	drop
	0 r !
;

hexdump 

