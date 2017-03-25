( ==================== Brain Fuck Compiler =================== )
( The following brain fuck compiler is based on one available here
http://rosettacode.org/wiki/Execute_Brain****/Forth. It has been
adapted to work with libforth. The license for this program is
GNU Free Documentation License, [v1.2], as it is derived from
the program on there.

Brain F*ck is a simple language with only 8 commands, a description
from Wikipedia [https://en.wikipedia.org/wiki/Brainfuck]:

	"The language consists of eight commands, listed below. A brainfuck
	program is a sequence of these commands, possibly interspersed with other
	characters [which are ignored]. The commands are executed sequentially,
	with some exceptions: an instruction pointer begins at the first command,
	and each command it points to is executed, after which it normally moves
	forward to the next command. The program terminates when the instruction
	pointer moves past the last command.

	The brainfuck language uses a simple machine model consisting of the
	program and instruction pointer, as well as an array of at least 30,000
	byte cells initialized to zero; a movable data pointer [initialized
	to point to the leftmost byte of the array]; and two streams of bytes
	for input and output [most often connected to a keyboard and a monitor
	respectively, and using the ASCII character encoding].

	> 	increment the data pointer [to point to the next cell to the
	  	right].
	< 	decrement the data pointer [to point to the next cell to the
	        left].
	+ 	increment the byte at the data pointer.
	- 	decrement the byte at the data pointer.
	. 	output the byte at the data pointer.
	, 	accept one byte of input, storing its value in the byte at
	        the data pointer.
	[ 	if the byte at the data pointer is zero, then instead of 
                moving the instruction pointer forward to the next command, 
                jump it forward to the command after the matching ] command.
	] 	if the byte at the data pointer is nonzero, then instead of 
                moving the instruction pointer forward to the next command, 
	        jump it back to the command after the matching [ command."

This implementation only provides a minimum of 2048 cells [on a 16-bit
computer], this should be sufficient for most programs.

The word 'bf' takes a [non-transient] string to a brain f*ck program and
parser a word from input, creating a new word and compiling the string
into the new word. 

The compiled world can be executed like any other Forth word. As an example,
the following program brain f*ck program implements an echo program:

	c" +[,.]" bf echo

Which can be run by typing in 'echo'.

Whilst this is a toy program it does elucidate how a compiler from one
language to another can be created in Forth.
)

1024 constant bf-len

: init ( -- c-addr u : initialize the transient region BF executes from ) 
	here bf-len erase chere 0 ;

: check ( c-addr : check that the data pointer is within bounds ) 
	chere bf-len chars> bounds swap within 0= if -1 throw then ;

: right ( c-addr u -- c-addr+1 u : move the data pointer right one BF cell ) 
	over c! 1+ dup dup check c@ ;

: left ( c-addr u -- c-addr-1 u : move the data pointer left one BF cell ) 
	over c! 1- dup dup check c@ ;

: bf-compile ( char -- : compile a single brain f*ck command )
	case
		[char] [ of 	postpone begin
				['] dup ,
				postpone while       endof
		[char] ] of	postpone repeat      endof
		[char] + of  	['] 1+ ,             endof
		[char] - of	['] 1- ,             endof
		[char] < of	['] left ,           endof
		[char] > of	['] right ,          endof
		[char] , of	['] drop , ['] key , endof
		[char] . of	['] dup , ['] emit , endof
\		[char] ; of     postpone ;           endof
	endcase ;

: (bf) c@ bf-compile ;

: bf ( c-addr u c" xxx" -- create a new word that executes a brain f*ck program )
	::
	['] init ,
	['] (bf) foreach	
	['] swap , ( swap pointer and current cell value )
	['] c! ,   ( write it back )
	(;) ;

hide{ bf-len bf-compile init check right left (bf) }hide

( An example creates a Forth word called 'hello' that prints "Hello, World!" 

c" ++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++." bf hello
hello )


( ==================== Brain Fuck Compiler =================== )
