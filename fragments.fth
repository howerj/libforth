( code fragments, nothing coherent here, nothing is guaranteed to work )
 
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

( ==================== brainfuck compiler ================= )
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
( ===================== BNF Parser ===================== )
\ See http://www.bradrodriguez.com/papers/bnfparse.htm
\ I find this parser amazing, a BNF parser in a single screen of
\ Forth

\ bnf parser                                (c) 1988 b. j. rodriguez
0 variable success
: <bnf   success @ if  r> in @ >r dp @ >r  >r
   else  r> drop  then ;
: bnf>   success @ if  r>  r> r> 2drop   >r
   else  r>  r> dp ! r> in !  >r then ;
: |    success @ if  r> r> r> 2drop drop
   else  r> r> r> 2dup >r >r in ! dp !  1 success !  >r then ;
: bnf:   [compile] : smudge compile <bnf ; immediate
: ;bnf   compile bnf> smudge [compile] ; ; immediate
: @token ( - n)   in @ tib @ + c@ ;
: +token ( f)    if 1 in +! then ;
: =token ( n)    success @ if @token =  dup success ! +token
   else drop then ;
: token ( n)    <builds c, does> ( a)  c@ =token ;

( see https://rosettacode.org/wiki/Tamagotchi_emulator
  this currently does not work, however it would be interesting
  to get the object system working )

( current object )
0 value o
' o >body constant 'o
: >o ( o -- ) postpone o postpone >r postpone 'o postpone ! ; immediate
: o> ( -- )   postpone r> postpone 'o postpone ! ; immediate
 
( chibi: classes with a current object and no formal methods )
0 constant object
: subclass ( class "name" -- a )  create here swap , does> @ ;
: class ( "name" -- a )  object subclass ;
: end-class ( a -- )  drop ;
: var ( a size "name" -- a )  over dup @ >r +!
  : postpone o r> postpone literal postpone + postpone ; ;
 
( tamagotchi )
class tama
  cell var hunger
  cell var boredom
  cell var age
  cell var hygiene
  cell var digestion
  cell var pooped
end-class
 
: offset ( -- )  \ go to column #13 of current line
  s\" \e[13g" type ;
 
: show ( "field" -- )
  ' postpone literal postpone dup
  postpone cr postpone id. postpone offset
  postpone execute postpone ? ;  immediate
: dump ( -- )
  show hunger  show boredom  show age  show hygiene
  cr ." pooped" offset pooped @ if ." yes" else ." no" then ;
 
\ these words both exit their caller on success
: -poop ( -- )
  digestion @ 1 <> ?exit  digestion off  pooped on
  cr ." tama poops!"  r> drop ;
: -hunger ( -- )
  digestion @ 0 <> ?exit  hunger ++
  cr ." tama's stomach growls"  r> drop ;
 
: died-from ( 'reason' f -- )
  if cr ." tama died from " type cr bye then 2drop ;
: by-boredom ( -- )  "boredom"  boredom @ 5 > died-from ;
: by-sickness ( -- ) "sickness" hygiene @ 1 < died-from ;
: by-hunger ( -- )   "hunger"   hunger @  5 > died-from ;
: by-oldness ( -- )  "age"      age @    30 > died-from ;
 
: sicken ( -- )  pooped @ if hygiene -- then ;
: digest ( -- )  -poop -hunger  digestion -- ;
: die ( -- )     by-boredom  by-sickness  by-hunger  by-oldness ;
 
( tamagotchi ops )
: spawn ( -- )
  cr ." tama is born!"
  hunger off  boredom off  age off  pooped off
  5 hygiene !  5 digestion ! ;
 
: wait ( -- )
  cr ." ** time passes **"
  boredom ++  age ++
  digest sicken die ;
 
: look ( -- )  0
  boredom @ 2 > if 1+ cr ." tama looks bored" then \ ( ◡‿◡ ) 
  hygiene @ 5 < if 1+ cr ." tama could use a wash" then \ (◕_◕)
  hunger @  0 > if 1+ cr ." tama's stomach is grumbling" then \ (⌣̀_⌣́)
  age @    20 > if 1+ cr ." tama is getting long in the tooth" then \ (-_-)
  pooped @      if 1+ cr ." tama is disgusted by its own waste" then \ (*μ_μ)
  0= if cr ." tama looks fine" then ; \ (⌒‿⌒)
 
: feed ( -- )
  hunger @ 0= if cr ." tama bats the offered food away" exit then \ (╮°-°)╮┳━━┳ ( ╯°□°)╯ ┻━━┻
  cr ." tama happily devours the offered food" \  ( o˘◡˘o) ┌iii┐
  hunger off  5 digestion ! ;
 
: clean ( -- )
  pooped @ 0= if cr ." tama is clean enough already." exit then \ ( ͠° ͟ʖ ͡°)
  cr ." you dispose of the mess."  pooped off  5 hygiene ! ; \  ╰( ͡° ͜ʖ͡° )つ──☆*:・ﾟ
 
: play ( -- )
  boredom @ 0= if cr ." tama ignores you." exit then \ (－_－) zzZ
  cr ." tama plays with you for a while."  boredom off ; \ ヽ(^o^)ρ┳┻┳°σ(^o^)ノ
 
( game mode )
\ this just permanently sets the current object
\ a more complex game would use >o ... o> to set it
create pet  tama allot
pet to o
 
cr .( you have a pet tamagotchi!)
cr
cr .( commands:  wait look feed clean play)
cr  ( secret commands: spawn dump )
spawn look
cr
