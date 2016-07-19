( code fragments, nothing coherent here )
 
: actual-base base @ dup 0= if drop 10 then ;
: pnum ( x -- : print number )
	dup
	actual-base mod [char] 0 +
	swap actual-base / dup
	if pnum 0 then
	drop emit ;

: . ( x -- : print number using pnum )
	dup 0 < 
	if
		[char] - emit negate
	then
	pnum
	space ; 

\ : overflow 2dup <> >r over - u>= r> and ;

: r13 ( c -- o : convert a character to ROT-13 form )
  >lower trip
	lowercase?
	if
		[char] m > if -13 else 13 then +
	else 
		drop 
	then ;

: r13-type ( c-addr u : print string in ROT-13 encoded form )
	bounds do i c@ r13 emit loop ;

( From http://rosettacode.org/wiki/Forth_common_practice )
: c+! ( n caddr -- ) dup >r c@ + r> c! ;
: append ( src len dest -- ) 2dup 2>r  count + swap move  2r> c+! ;
: place ( src len dest -- ) 2dup 2>r  1+ swap move  2r> c! ;
: scan ( str len char -- str' len' ) >r begin dup while over c@ r@ <> while 1 /string repeat then r> drop ;
: skip ( str len char -- str' len' ) >r begin dup while over c@ r@ =  while 1 /string repeat then r> drop ;
: split ( str len char -- str1 len1 str2 len2 ) >r 2dup r> scan 2swap 2 pick - ;

( a non portable way of making the terminal input raw )
: raw c" /bin/stty raw" system ;
: cooked c" /bin/stty cooked" system ;

( HERE documents would be good to implement, see
https://rosettacode.org/wiki/Here_document#Forth )

( from https://rosettacode.org/wiki/History_variables#Forth, )
: history postpone create here cell+ , 0 , -1 , ;
: h@ @ @ ;
: h! swap here >r , dup @ , r> swap ! ;
: .history @ begin dup cell+ @ -1 <> while dup ? cell+ @ repeat drop ;
: h-- dup @ cell+ @ dup -1 = if abort ( abort" End of history" ) then swap ! ;

( \ Example use: 
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
z dup h-- h@ . )
