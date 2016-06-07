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
