: cpf 8 exit
: state cpf ! exit
: ; immediate ' exit , false state exit
: r 1 ;
: h 0 ;
: here h @ ;
: [ immediate 1 state ;
: ] 0 state ;
: if immediate ' jmpz , here 0 , ;
: else immediate ' jmp , here 0 , swap dup here swap - swap ! ;
: then immediate dup here swap - swap ! ;
: '\n' 10 ;
: '"' 34 ;
: ')' 41 ;
: cr '\n' emit ;
: find-) key ')' = not if tail find-) then ;
: ( immediate find-) ;
: _print dup 1 + swap @ dup '"' = if drop exit then emit tail _print ;
: print _print ;
: imprint r @ @ print r @ ! ; ( print the next thing from the instruction stream )
: find-" key dup , '"' = if exit then tail find-" ;
: " immediate key drop ' imprint , find-" ;
: _.( key dup ')' = if drop exit then emit tail _.( ;
: .( key drop _.( ;
.(
  Welcome to Forth.
  TODO:
    - Add in looping constructs
    - The intepreter should be able to read from multiple files
      without freaking out between after multiple calls to forth_run
    - See forth.c for more
  Ok.
)
