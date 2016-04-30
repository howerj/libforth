#!./forth

: not 0= ;
: <> = not ;
: find-) key ')' <> if tail find-) then ;
: ( immediate find-) ;

( CREATE DOES> )
: _push 2 ; ( @bug really "@hack", this should be 0, not 2! )
: _run 2 ;
: false 0 ;
: true 1 ;

: write-quote ' ' , ;   ( A word that writes ' into the dictionary )
: write-comma ' , , ;   ( A word that writes , into the dictionary )
: write-exit ' exit , ; ( A word that write exit into the dictionary )
: 2+ 2 + ;
: 3+ 2+ 1+ ;
: cpf 8 ; ( compile flag, used in "state" )

\ This word should be hidden when possible
: _create        \ create a new work that pushes its data field
   ::            \ compile a word
   _push ,       \ write push into new word
   here 2+ ,     \ push a pointer to data field
   ' exit ,      \ write in an exit to new word (data field is after exit) 
   false state   \ return to command mode
;

\ A simple version of create is as follows  
\   : create :: 2 , here 2 + , ' exit , 0 state ; 
\ But this version is much more limited
: create immediate              \ This is a complicated word! It makes a
                                \ word that makes a word.
  cpf @ if                      \ Compile time behavior
  ' :: ,                        \ Make the defining word compile a header
  write-quote _push , write-comma    \ Write in push to the creating word
  ' here , ' 3+ , write-comma        \ Write in the number we want the created word to push
  write-quote here 0 , write-comma   \ Write in a place holder (0) and push a 
                                     \ pointer to to be used by does>
  write-quote write-exit write-comma                \ Write in an exit in the word we're compiling.
  ' false , ' state ,           \ Make sure to change the state back to command mode
  else                          \ Run time behavior
        _create
  then
;

: does> immediate
  write-exit                    \ Write in an exit, we don't want the
                                \ defining to run it, but the *defined* word to.
   here swap !                  \ Patch in the code fields to point to.
  _run ,                        \ Write a run in.
;

: array    create allot does> + ;
: variable create ,     does>   ;
: constant create ,     does> @ ;

( UTILITY )

: logical not not ;
: 2drop drop drop ;
: ?dup dup if dup then ;
: > 2dup = if 2drop false else < if false else true then then ;
: min 2dup < if drop else swap drop then  ; 
: max 2dup > if drop else swap drop then  ; 
: >= < not ;
: <= > not ;
: 2* 1 lshift ;
: 2/ 1 rshift ;
: # dup . cr ;

: log-2 0 swap begin swap 1+ swap 2/ dup 0= until drop 1- ;

( : 8* 8 * ;
: mask 8* 255 swap lshift ;
: size-1 [ size 1- literal ] ;
: alignment-bits size-1 and ;
: caddr [ size log-2 literal ] rshift ;
: c@ dup caddr @ swap alignment-bits tuck mask and swap 8* rshift ;
: c! tuck dup caddr @ swap alignment-bits rot swap  8* lshift or swap caddr ! ;
1 hex
1 0x4000 !
0xF0 0x8000 c! 
0xC0 0x8001 c!
0x4000 @ . cr )

0 variable i
0 variable j

: i! i ! ;
: j! j ! ;
: i@ i @ ;
: j@ j @ ;

: do immediate 
  ' j! ,
  ' i! ,
  here
;

: >= 
  < not
;

: (++i)>=j
  i@ 1+ i! i@ j@ >= 
;

: loop immediate
  ' (++i)>=j ,
  ' jz ,
  here - ,
;

: break 
  j@ i!
;


\ ============================================================================
\ UTILITY
\ ============================================================================


