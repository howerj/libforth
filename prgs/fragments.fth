( code fragments, nothing coherent here )
 
: actual-base base @ dup 0= if drop 10 then ;
: pnum
	dup
	actual-base mod [char] 0 +
	swap actual-base / dup
	if pnum 0 then
	drop emit
;

: .
	dup 0 < 
	if
		[char] - emit negate
	then
	pnum
	space
; 


