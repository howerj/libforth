#!./forth
\ @bug Test code only, this needs refactoring
: _push 2 ; \ @bug (really @hack) this should be 0, not 2!
: _run 2 ;
: false 0 ;

: '', ' ' , ;       \ A word that writes ' into the dictionary
: ',, ' , , ;       \ A word that writes , into the dictionary
: 'exit, ' exit , ; \ A word that write exit into the dictionary
: 2+ 2 + ;
: 3+ 2+ 1+ ;
: cpf 8 ; \ compile flag, used in "state"

: _create        \ create a new work that pushes its data field
   ::            \ compile a word
   _push ,       \ write push into new word
   here 2+ ,     \ push a pointer to data field
   ' exit ,      \ write in an exit to new word (data field is after exit) 
   false state   \ return to command mode
;

: create immediate              \ This is a complicated word! It makes a
                                \ word that makes a word.
  cpf @ if                      \ Compile time behavior
  ' :: ,                        \ Make the defining word compile a header
  '', _push , ',,               \ Write in push to the creating word
  ' here , ' 3+ , ',,           \ Write in the number we want the created word to push
  '', here 0 , ',,              \ Write in a place holder (0) and push a 
                                \ pointer to to be used by does>
  '', 'exit, ',,                \ Write in an exit in the word we're compiling.
  ' false , ' state ,           \ Make sure to change the state back to command mode
  else                          \ Run time behavior
        _create
  then
;

: does> immediate
  'exit,                        \ Write in an exit, we don't want the
                                \ defining to run it, but the *defined* word to.
   here swap !                  \ Patch in the code fields to point to.
  _run ,                        \ Write a run in.
;

: array    create allot does> + ;
: variable create ,     does>   ;
: constant create ,     does> @ ;

\ .( Debug information ) cr
\ 8 array c
\ -1 , \ marker, for out viewing pleasure only
\ 
\ .( array definition ) cr
\ find array dup 20 + list
\ .( defined array 'c' ) cr
\ find c dup 20 + list
\ .( '+'    ) find + . cr
\ .( 'exit' ) find exit . cr 

\ Simple Create -> : create :: 2 , here 2 + , ' exit , 0 state ; ";


