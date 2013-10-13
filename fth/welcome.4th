( =========================================================================== )
( === Welcome Banner and banner words ======================================= )
( =========================================================================== )

( TODO : Add more screen manipulation words )

( ANSI terminal color codes )
: esc 'esc' emit ;
: rst esc ." [2J" cr ;    \ Clear screen
: clr esc ." [0;0H" cr ;  \ Set cursor to 0,0
: red esc ." [31;1m" ;    \ Change text color to red.
: grn esc ." [32;1m" ;    \ ...to green.
: blu esc ." [34;1m" ;    \ ...to blue.
: nrm esc ." [0m" cr ;    \ ...to the default.

: cursor ( x y -- ) esc ." [" prnn ." ;" prnn ." H" fflush kernel ;

( Welcome message. )
rst clr grn
.( Howe Forth ) cr .( Base System Loaded ) cr
.( @author         ) blu .( Richard James Howe. ) grn cr
.( @copyright      ) blu .( Copyright 2013 Richard James Howe. ) grn cr
.( @license        ) blu .( LGPL ) grn cr
.( @email          ) blu .( howe.r.j.89@gmail.com ) grn cr
.( Memory Used: ) cr 
.(   Dictionary:   ) blu here 4 * str @reg + . grn
.(   Strings:      ) blu str @reg . grn
red .( OK ) nrm
