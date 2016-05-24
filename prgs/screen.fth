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

