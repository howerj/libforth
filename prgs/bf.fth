\ brainfuck compiler
\ see https://rosettacode.org/wiki/Execute_Brain****/Forth
\ This needs work

\ brainfuck compiler

1024 constant size
: init  ( -- p *p ) here size erase  here 0 ;
: right ( p *p -- p+1 *p ) over c!  1+  dup c@ ;
: left  ( p *p -- p-1 *p ) over c!  1-  dup c@ ;		\ range check?

: compile-bf-char ( c -- )
  case
  [char] [ of postpone begin
              postpone dup
              postpone while  endof
  [char] ] of postpone repeat endof
  [char] + of postpone 1+     endof
  [char] - of postpone 1-     endof
  [char] > of postpone right  endof
  [char] < of postpone left   endof
  [char] , of postpone drop
              postpone key    endof
  [char] . of postpone dup
              postpone emit   endof
    \ ignore all other characters
  endcase ;

: compile-bf-string ( addr len -- )
  postpone init
  bounds do i c@ compile-bf-char loop
  postpone swap
  postpone c!
  postpone ;
;

.( here ) cr
: :bf ( name " bfcode" -- )
  ::
  char parse    \ get string delimiter
  compile-bf-string ;
:bf printA " ++++++[>+++++++++++<-]>-."

printA

( 
	: :bf-file \ name file -- 
	  :
	  bl parse slurp-file
	  compile-bf-string ;
)
