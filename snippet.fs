: sw find dup 20 + show ;

: ?dup dup if dup then ;
: negate -1 * ;
: abs dup 0 < if negate then ;

\ CREATE ========================================================
\ Working create / does> !
\ Needs to be made immediate.
: @x 11 @reg ;
: !x 11 !reg ;
: @y 12 @reg ;
: !y 12 !reg ;

: create 
  ::
  _push , \ push the next value
  here 5 + , \ points to data field.
  0 ,     \ place holder
  here !x \ Store current dictionary pointer to be patched.
  _push , \ push the next value
  ' >r , \ make the defined word execute value on stack.
  ' exit ,
  false state
;

: does> 
  @x \ Fetch address to patch
	r> dup >r swap !dic \ Patch address with next value in line
  tail \ Drop out here so we don't execute anything after the does>
;

: var create , does> ;
: const create , does> @dic ;
: array create allot does> + ;
\ CREATE ========================================================

