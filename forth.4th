: state 8 ! exit
: ; immediate ' exit , 0 state exit
: r 1 ;
: h 0 ;
: here h @ ;
: [ immediate 1 state ;
: ] 0 state ;
: if immediate ' jmpz , here 0 , ;
: else immediate ' jmp , here 0 , swap dup here swap - swap ! ;
: then immediate dup here swap - swap ! ;
: begin immediate here ;
: until immediate ' jmpz , here - , ;
: halt 2 >r ;
: 0= 0 = ;
: '\n' 10 ;
: '"' 34 ;
: ')' 41 ;
: cr '\n' emit ;
: ( immediate begin key ')' = if exit then 0 until ;
: print begin dup 1 + swap @ dup '"' = if drop exit then emit 0 until ;
: imprint r> print >r ; 
: find-" key dup , '"' = if exit then tail find-" ;
: " immediate key drop ' imprint , find-" ;
: .( key drop begin key dup ')' = if drop exit then emit 0 until ;
: dump 32 begin 1 - dup dup 1024 * swap save drop dup 0= until ;
.( OK. ) here . cr
