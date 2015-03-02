: state 8 ! exit
: ; immediate ' exit , 0 state exit
: hex 9 ! ;
: r 1 ;
: h 0 ;
: here h @ ;
: [ immediate 0 state ;
: ] 1 state ;
: :noname immediate here 2 , ] ;
: if immediate ' jz , here 0 , ;
: else immediate ' j , here 0 , swap dup here swap - swap ! ;
: then immediate dup here swap - swap ! ;
: begin immediate here ;
: until immediate ' jz , here - , ;
: halt 2 >r ;
: 0= 0 = ;
: 1+ 1 + ;
: '"' 34 ;
: ')' 41 ;
: tab 9 emit ;
: cr 10 emit ;
: print begin dup 1+ swap @ dup '"' = if drop exit then emit 0 until ;
: imprint r> print >r ; 
: " immediate key drop ' imprint , begin key dup , '"' = until ;
: .( key drop begin key dup ')' = if drop exit then emit 0 until ;
: dump 32 begin 1 - dup dup 1024 * swap save drop dup 0= until ;
: 2dup dup >r >r dup r> swap r> ;
: line dup . tab dup 4 + swap begin dup @ . tab 1+ 2dup = until drop ;
: list swap begin line cr 2dup < 0= until ;
.( OK. ) here . cr
