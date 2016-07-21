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

( \ Heres an interesting piece of code from http://c2.com/cgi/wiki?ForthSimplicity

 : IMMEDIATE?	-1 = ;
 : NEXTWORD	BL WORD FIND ;
 : NUMBER,	NUMBER POSTPONE LITERAL ;
 : COMPILEWORD	DUP IF IMMEDIATE? IF EXECUTE ELSE COMPILE, THEN ELSE NUMBER, THEN ;
 : ]	BEGIN NEXTWORD COMPILEWORD AGAIN ;
 : [	R> R> 2DROP ; IMMEDIATE	\ Breaks out of compiler into interpret mode again )

( ===== SCREEN SAVER ===== )
( requires forth.fth )

: make-star     [char] * emit ;
: make-stars    0 do make-star loop cr ;
: make-square   dup 0 do dup make-stars loop drop ;
: make-triangle 1 do i make-stars loop ;
: make-tower    dup make-triangle make-square ;

0 variable x
0 variable y
4 variable scroll-speed
16 variable paint-speed
10 variable wait-time

: @x random 80 mod x ! x @ ;
: @y random 40 mod y ! y @ ;
: maybe-scroll random scroll-speed @ mod 0= if cr then ;
: star [char] * emit ;
: paint @x @y at-xy star ;
: maybe-paint random paint-speed @ mod 0= if paint then ;
: wait wait-time @ ms ;
: screen-saver
	page
	hide-cursor
	begin
		maybe-scroll
		maybe-paint
		wait
	again ;

( ===== Levenshtein distance ===== )
( https://rosettacode.org/wiki/Levenshtein_distance#Forth )
: levenshtein                          ( a1 n1 a2 n2 -- n3)
  dup                                  \ if either string is empty, difference
  if                                   \ is inserting all chars from the other
    2>r dup
    if
      2dup 1- chars + c@ 2r@ 1- chars + c@ =
      if
        1- 2r> 1- recurse exit
      else                             \ else try:
        2dup 1- 2r@ 1- recurse -rot    \   changing first letter of s to t;
        2dup    2r@ 1- recurse -rot    \   remove first letter of s;
        1- 2r> recurse min min 1+      \   remove first letter of t,
      then                             \ any of which is 1 edit plus 
    else                               \ editing the rest of the strings
      2drop 2r> nip
    then
  else
    2drop nip
  then
;
 
c" kitten"      c" sitting"       levenshtein . cr
c" rosettacode" c" raisethysword" levenshtein . cr


: star 42 emit ;
: top 0 do star loop cr ; : bottom top ;
: middle star 2 - 0 do space loop star cr ;
: box ( width height -- ) cr over top 2 - 0 do dup middle loop bottom ;


