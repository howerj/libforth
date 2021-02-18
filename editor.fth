( ==================== Block Editor ========================== )
( Experimental block editor Mark II

This is a simple block editor, it really shows off the power
and simplicity of Forth systems - both the concept of a block
editor and the implementation.

Forth source code used to organized into things called 'blocks',
nowadays most people use normal resizeable stream based files.
A block simply consists of 1024 bytes of data that can be
transfered to and from mass storage. This works on systems that
do not have a file system. A block can be used for storing
arbitrary data as well, so the user had to know what was stored
in what block.

1024 bytes is quite a limiting Form factor for storing source
code, which is probably one reason Forth earned a reputation
for being terse. A few conventions arose around dealing with
Forth source code stored in blocks - both in how the code should
be formatted within them, and in how programs that required
more than one block of storage should be dealt with.

One of the conventions is how many columns and rows each block
consists of, the traditional way is to organize the code into
16 lines of text, with a column width of 64 characters. The only
space character used is the space [or ASCII character 32], tabs
and new lines are not used, as they are not needed - the editor
knows how long a line is so it can wrap the text.

Various standards and common practice evolved as to what goes
into a block - for example the first line is usually a comment
describing what the block does, with the date is was last edited
and who edited it.

The way the editor works is by defining a simple set of words
that act on the currently loaded block, LIST is used to display
the loaded block. An editor loop which executes forth words
and automatically displays the currently block can be entered
by calling EDITOR. This loop is essential an extension of
the INTERPRET word, it does no special processing such as
looking for keys - all commands are Forth words. The editor
loop could have been implemented in the following fashion
[or something similar]:

	: editor-command
		key
		case
			[char] h of print-editor-help endof
			[char] i of insert-text endof
			[char] d of delete-line endof
			... more editor commands
			[char] q of quit-editor-loop endof
			push-number-by-default
		endof ;

However this is rather limiting, it only works for single
character commands and numeric input is handled as a special
case. It is far simpler to just call the READ word with the
editor commands in search order, and it can be extended not
by adding more parsing code in CASE statements but just by
defining new words in the ordinary manner.

The editor loop does not have to be used while editing code,
but it does make things easier. The commands defined can be
used on there own.
	
The code was adapted from: 	

http://retroforth.org/pages/?PortsOfRetroEditor

Which contains ports of an editor written for Retro Forth.

- '\' needs extending to work with the block editor, for now,
use parenthesis for comments
- add multi line insertion mode
- Add to an editor vocabulary, which will need the vocabulary
system to exist.
- Using PAGE should be optional as not all terminals support
ANSI escape codes - thanks to CMD.EXE. Damned Windows.

Adapted from http://retroforth.org/pages/?PortsOfRetroEditor )


: help
page cr
" Block Editor Help Menu

      n    move to next block
      p    move to previous block
    # d    delete line in current block
      x    erase current block
      e    evaluate current block
    # i    insert line
 # #2 ia   insert at line #2 at column #
      q    quit editor loop
    # b    set block number
      s    save block and write it out
      u    update block

 -- press any key to continue -- " cr ( " )
char drop ;

: (block) blk @ block ;
: (check) dup b/buf c/l / u>= if -24 throw then ;
: (line) (check) c/l * (block) + ; ( n -- c-addr : push current line address )
: (clean)
	(block) b/buf
	2dup nl bl subst
	2dup cret bl subst
	      0 bl subst ;
: n  1 +block block ;
: p -1 +block block ;
: d (line) c/l bl fill ;
: x (block) b/buf bl fill ;
: s update save-buffers ;
: q rdrop rdrop ;
: e blk @ load char drop ;
: ia c/l * + dup b/buf swap - >r (block) + r> accept drop (clean) ;
: i 0 swap ia ;
: editor
	1 block     ( load first block by default )
	rendezvous  ( set up a rendezvous so we can forget words up to this point )
	begin
		page cr
		" BLOCK EDITOR: TYPE 'HELP' FOR A LIST OF COMMANDS" cr
		blk @ list
		" [BLOCK: " blk @ u. " ] " 
		" [HERE: " here u. " ] " 
		" [SAVED: " blk @ updated? not u. " ] "
		cr
		postpone [ ( need to be in command mode )
		read
	again ;

( Extra niceties )
c/l string yank 
yank bl fill
: u update ;
: b block ;
: l blk @ list ;
: y (line) yank >r swap r> cmove ; ( n -- yank line number to buffer )
: c (line) yank cmove ; ( n -- copy yank buffer to line )
: ct swap y c ; ( n1 n2 -- copy line n1 to n2 )
: ea (line) c/l evaluate throw ;
: m retreat ; ( -- : forget everything since editor session began )

: sw 2dup y (line) swap (line) swap c/l cmove c ;

hide{ (block) (line) (clean) yank }hide

( ==================== Block Editor ========================== )

