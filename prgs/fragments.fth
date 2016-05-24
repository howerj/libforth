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

( HERE documents would be good to implement, see
https://rosettacode.org/wiki/Here_document#Forth )

( from https://rosettacode.org/wiki/History_variables#Forth,
  )
: history postpone create here cell+ , 0 , -1 , ;
: h@ @ @ ;
: h! swap here >r , dup @ , r> swap ! ;
: .history @ begin dup cell+ @ -1 <> while dup ? cell+ @ repeat drop ;
: h-- dup @ cell+ @ dup -1 = if abort ( abort" End of history" ) then swap ! ;

( Example use: 
history z  
23 z h!  
z h@ . 
34 z h!  
z h@ . 
45 z h!  
z h@ . 
z .history 
z dup h-- h@ . 
z dup h-- h@ .
z dup h-- h@ .
z dup h-- h@ .
)
