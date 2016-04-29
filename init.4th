#!./forth
\ ============================================================================
\ START CREATE
\ ============================================================================
\ @bug Test code only, this needs refactoring
: _push 2 ; \ @bug (really @hack) this should be 0, not 2!
: _run 2 ;
: false 0 ;
: true 1 ;

: write-quote ' ' , ;   \ A word that writes ' into the dictionary
: write-comma ' , , ;   \ A word that writes , into the dictionary
: write-exit ' exit , ; \ A word that write exit into the dictionary
: 2+ 2 + ;
: 3+ 2+ 1+ ;
: cpf 8 ; \ compile flag, used in "state"

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

\ ============================================================================
\ END CREATE
\ ============================================================================

\ ============================================================================
\ UTILITY
\ ============================================================================

: 2drop drop drop ;
: ?dup dup if dup then ;
: > 2dup = if 2drop false else < if false else true then then ;
: min 2dup < if drop else swap drop then  ; 
: max 2dup > if drop else swap drop then  ; 

\ ============================================================================
\ UTILITY
\ ============================================================================


