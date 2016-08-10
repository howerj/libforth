/**@file       libforth.c
 * @brief      A FORTH library, written in a literate style
 * @author     Richard James Howe.
 * @copyright  Copyright 2015,2016 Richard James Howe.
 * @license    MIT (see https://opensource.org/licenses/MIT)
 * @email      howe.r.j.89@gmail.com
 * @todo Add in as many checks as possible, such as checks to make sure the
 * dictionary pointer does not cross into the stack space.
 * @todo cxxforth (see https://github.com/kristopherjohnson/cxxforth) uses
 * a preprocessor to turn a literate C++ file into markdown, this project
 * should do the same.
 * @todo The file access functions need improving, SOURCE-ID needs extending
 * and some Forth words can be reimplemented in terms of the file access
 * functions. The bsave and bload can be removed.
 * @todo Add save-core, number, word (or parse), load-core, more-core to the
 * virtual machine.
 * @todo Add loading in a Forth image from a memory structure, this will need
 * to be in a portable Format.
 * @todo Add a C FFI and methods of adding C functions to the interpreter.
 * @todo Error handling could be improved - the latest word definition should be
 * erased if an error occurs before the terminating ';'.
 * @todo Make a compiler (a separate program) that targets the Forth virtual
 * machine.
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Richard James Howe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 *  This file implements the core Forth interpreter, it is written in portable
 *  C99. The file contains a virtual machine that can interpret threaded Forth
 *  code and a simple compiler for the virtual machine, which is one of its
 *  instructions. The interpreter can be embedded in another application and
 *  there should be no problem instantiating multiple instances of the
 *  interpreter.
 *
 *  For more information about Forth see:
 *
 *  - https://en.wikipedia.org/wiki/Forth_%28programming_language%29
 *  - Thinking Forth by Leo Brodie
 *  - Starting Forth by Leo Brodie
 *
 *  The antecedent of this interpreter:
 *  - http://www.ioccc.org/1992/buzzard.2.c
 *
 *  cxxforth, a literate Forth written in C++
 *  - https://github.com/kristopherjohnson/cxxforth
 *
 *  Jones Forth, a literate Forth written in x86 assembly:
 *  - https://rwmj.wordpress.com/2010/08/07/jonesforth-git-repository/
 *  - https://github.com/AlexandreAbreu/jonesforth (backup)
 *
 *  A Forth processor:
 *  - http://www.excamera.com/sphinx/fpga-j1.html
 *  And my Forth processor based on this one:
 *  - https://github.com/howerj/fyp
 *
 *  The repository should also contain:
 *
 *  - "readme.md"  : a Forth manual, and generic project information
 *  - "forth.fth"  : basic Forth routines and startup code
 *  - "libforth.h" : The header contains the API documentation
 *
 *  The structure of this file is as follows:
 *
 *  1) Headers and configuration macros
 *  2) Enumerations and constants
 *  3) Helping functions for the compiler
 *  4) API related functions and Initialization code
 *  5) The Forth virtual machine itself
 *  6) An example main function called main_forth and support
 *  functions
 *
 *  Each section will be explained in detail as it is encountered.
 *
 *  An attempt has been made to make this document flow, as both a source
 *  code document and as a description of how the Forth kernel works.
 *  This is helped by the fact that the program is quite small and compact
 *  without being written in obfuscated C. It is, as mentioned, compact,
 *  and can be quite difficult to understand regardless of code quality. There
 *  are a number of behaviors programmers from a C background will not be familiar
 *  with.
 *
 *  A basic understanding of how to use Forth would help as this document is
 *  meant to describe how a Forth implementation works and not as an
 *  introduction to the language. A quote about the language from Wikipedia
 *  best sums the language up:
 *
 *       "Forth is an imperative stack-based computer programming language
 *      and programming environment.
 *
 *      Language features include structured programming, reflection (the
 *      ability to modify the program structure during program execution),
 *      concatenative programming (functions are composed with juxtaposition)
 *      and extensibility (the programmer can create new commands).
 *
 *      ...
 *
 *      A procedural programming language without type checking, Forth features
 *      both interactive execution of commands (making it suitable as a shell
 *      for systems that lack a more formal operating system) and the ability
 *      to compile sequences of commands for later execution."
 *
 *  Forth has a philosophy like most languages, one of simplicity, compactness
 *  and of trying only to solve the problem at hand, even going as far as to try
 *  to simplify the problem or replace the problem (which may span multiple
 *  domains, not just software) completely with a simpler one. This is often not
 *  a realistic way of tackling things and Forth has largely fallen out of
 *  favor, it is nonetheless a very interesting language which can be
 *  implemented and understood by a single programmer (another integral part
 *  of the Forth philosophy).
 *
 *  The core of the concept of the language - simplicity I would say - is
 *  achieved by the following:
 *
 *  1) The language uses Reverse Polish Notation to enter expressions and parsing
 *  is simplified to the extreme with space delimited words and numbers being
 *  the most complex terms. This means a abstract syntax tree does not need to
 *  be constructed and terms can be executed as soon as they are parsed. The
 *  "parser" can described in only a handful of lines of C.
 *  2) The language uses concatenation of Forth words (called functions in
 *  other language) to create new words, this allows for very small programs to
 *  be created and encourages "factoring" definitions into smaller words.
 *  3) The language is untyped.
 *  4) Forth functions, or words, take their arguments implicitly and return
 *  variables implicitly via a variable stack which the programmer explicitly
 *  interacts with. A comparison of two languages behavior best illustrates the
 *  point, we will define a function in C and in Forth that simply doubles a
 *  number. In C this would be:
 *
 *     int double_number(int x)
 *     {
 *         return x << 1;
 *     }
 *
 *  And in Forth it would be:
 *
 *     : 2* 1 lshift ;
 *
 *  No types are needed, and the arguments and the return values are not
 *  stated, unlike in C. Although this has the advantage of brevity, it is now
 *  up to the programmer to manages those variables.
 *
 *  5) The input and output facilities are set up and used implicitly as well.
 *  Input is taken from stdin and output goes to stdout, by default. Words that
 *  deal with I/O uses these file steams internally.
 *  6) Error handling is traditionally non existent or only very limited.
 *  7) This point is not a property of the language, but part of the way the
 *  Forth programmer must program. The programmer must make their factored word
 *  definitions "flow". Instead of reordering the contents of the stack for
 *  each word, words should be made so that the reordering does not have to
 *  take place (ie. Manually performing the job of a optimizing compile another
 *  common theme in Forth, this time with memory reordering).
 *
 *  The implicit behavior relating to argument passing and I/O really reduce
 *  program size, the type of implicit behavior built into a language can
 *  really define what that language is good for. For example AWK is naturally
 *  good for processing text, thanks in large part to sensible defaults for how
 *  text is split up into lines and records, and how input and output is
 *  already set up for the programmer.
 *
 *  Naturally we try to adhere to Forth philosophy, but also to Unix philosophy
 *  (which most Forths do not do), this is described later on.
 *
 *  Glossary of Terms:
 *
 *  VM             - Virtual Machine
 *  Cell           - The Virtual Machines natural Word Size, on a 32 bit
 *                 machine the Cell will be 32 bits wide
 *  Word           - In Forth a Word refers to a function, and not the
 *                 usual meaning of an integer that is the same size as
 *                 the machines underlying word size, this can cause confusion
 *  API            - Application Program Interface
 *  interpreter    - as in byte code interpreter, largely synonymous with virtual
 *                 machine as is used here
 *  REPL           - Read-Evaluate-Print-Loop, this Forth actually provides
 *                 something more like a "REL", or Read-Evaluate-Loop (as printing
 *                 has to be done explicitly), but the interpreter is interactive
 *                 which is the important point
 *  RPN            - Reverse Polish Notation (see
 *                 https://en.wikipedia.org/wiki/Reverse_Polish_notation).
 *                 The Forth interpreter uses RPN to enter expressions.
 *  The stack      - Forth implementations have at least two stacks, one for
 *                 storing variables and another for control flow and temporary
 *                 variables, when the term "stack" is used on its own and with
 *                 no other context it refers to the "variable stack" and not
 *                 the "return stack". This "variable stack" is used for
 *                 passing parameters into and return values to functions.
 *  Return stack   - Most programming languages have a call stack, C has one
 *                 but not one that the programmer can directly access, in
 *                 Forth manipulating the return stack is often used.
 *  factor         - factoring is splitting words into smaller words that
 *                 perform a specific function. To say a word is a natural
 *                 factor of another word is to say that it makes sense to take
 *                 some functionality of the word to be factored and to create
 *                 a new word that encapsulates that functionality. Forth
 *                 encourages heavy factoring of definitions.
 *  Command mode   - This mode executes both compiling words and immediate
 *                 words as they are encountered
 *  Compile mode   - This mode executes immediate words as they are
 *                 encountered, but compiling words are compiled into the
 *                 dictionary.
 *  Primitive      - A word whose instruction is built into the VM.
 **/

/* ============================ Section 1 ================================== */
/*                     Headers and configurations macros                     */

/* This file implements a Forth library, so a Forth interpreter can be embedded
 * in another application, as such many of the functions in this file are
 * exported, and are documented in the "libforth.h" header */
#include "libforth.h"

/* We try to make good use of the C library as even microcontrollers
 * have enough space for a reasonable implementation of it, although
 * it might require some setup. The only time allocations are explicitly
 * done is when the virtual machine image is initialized, after this
 * the VM does not allocate any more memory. */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>

/* Traditionally Forth implementations were the only program running on
 * the (micro)computer, running on processors orders of magnitude slower
 * than this one, as such checks to make sure memory access was in bounds
 * did not make sense and the implementation had to have access to the
 * entire machines limited memory.
 *
 * To aide debugging and to help ensure correctness the "ck" macro, a wrapper
 * around the function "check_bounds", is called for most memory accesses
 * that the virtual machine makes. */
#ifndef NDEBUG
/**@brief This is a wrapper around check_bounds, so we do not have to keep
 * typing in the line number, as so the name is shorter (and hence the checks
 * are out of the way visually when reading the code).
 * @param c expression to bounds check */
#define ck(c) check_bounds(o, &on_error, c, __LINE__, o->core_size)
/**@brief This is a wrapper around check_bounds, so we do not have to keep
 * typing in the line number, as so the name is shorter (and hence the checks
 * are out of the way visually when reading the code). This will check
 * character pointers instead of cell pointers, like "ck" does.
 * @param c expression to bounds check */
#define ckchar(c) check_bounds(o, &on_error, c, __LINE__, o->core_size * sizeof(forth_cell_t))
/**@brief This is a wrapper around check_depth, to make checking the depth
 * short and simple.
 * @param depth */
#define cd(depth) check_depth(o, &on_error, S, depth, __LINE__)
/**@brief This performs tracing
 * @param ENV forth environment
 * @param STK stack pointer
 * @param EXPECTED expected stack value
 * @param TOP current top of stack to print out*/
#define TRACE(ENV,INSTRUCTION,STK,TOP) trace(ENV,INSTRUCTION,STK,TOP)
#else
/**@brief This removes the bounds check and debugging code
 * @param c do nothing */
#define ck(c) (c)
/**@brief This removes the bounds check and debugging code
 * @param c do nothing */
#define ckchar(c) (c)
/**@brief This is a wrapper around check_depth
 * @param depth do nothing */
#define cd(depth) ((void)depth)
/**@brief This removes tracing
 * @param ENV
 * @param STK
 * @param EXPECTED 
 * @param TOP */
#define TRACE(ENV, INSTRUCTION, STK, TOP)
#endif

#define DEFAULT_CORE_SIZE   (32 * 1024) /**< default VM size */

/**@brief Blocks will be encountered and explained later, they have a fixed
 * size which has been standardized to 1024 */
#define BLOCK_SIZE          (1024u)

/**@brief When we are reading input to be parsed we need a space to hold that
 * input, the offset to this area is into a field called "m" in "struct forth",
 * defined later, the offset is a multiple of cells and not chars.  */
#define STRING_OFFSET       (32u)

/**@brief This defines the maximum length of a Forth words name, that is the
 * string that represents a Forth word, this number is in cells (or machine
 * words). */
#define MAXIMUM_WORD_LENGTH (32u)

/**@brief minimum stack size of both the variable and return stack, the stack
 * size should not be made smaller than this otherwise the built in code and
 * code in "forth.fth" will not work */
#define MINIMUM_STACK_SIZE  (64u)

/* The start of the dictionary is after the registers and the STRING_OFFSET,
 * this is the area where Forth definitions are placed. */
#define DICTIONARY_START    (STRING_OFFSET + MAXIMUM_WORD_LENGTH) /**< start of dic */

/* Later we will encounter a field called MISC, a field in every Word
 * definition and is always present in the Words header. This field contains
 * multiple values at different bit offsets, only the lower 16 bits of this
 * cell are ever used. The next macros are helper to extract information from
 * the MISC field. */

#define WORD_LENGTH_OFFSET  (8)  /**< bit offset for word length start */

/**@brief WORD_LENGTH extracts the length of a Forth words name so we know
 * where it is relative to the PWD field of a word
 * @param MISC This should be the MISC field of a word */
#define WORD_LENGTH(MISC) (((MISC) >> WORD_LENGTH_OFFSET) & 0xff)
#define WORD_HIDDEN(MISC) ((MISC) & 0x80) /**< is a forth word hidden? */

/**@brief The lower 7 bits of the MISC field are used for the VM instruction,
 * limiting the number of instructions the virtual machine can have in it, the
 * higher bits are used for other purposes */
#define INSTRUCTION_MASK    (0x7f)

/**@brief A mask that the VM uses to extract the instruction
 * @param k This MISC, or a CODE Field of a Forth word */
#define instruction(k)      ((k) & INSTRUCTION_MASK)

/**@brief VERIFY is our assert macro that will always been defined regardless of
 * whether NDEBUG is defined
 * @param X expression to verify*/
#define VERIFY(X)           do { if(!(X)) { abort(); } } while(0)

/**@brief The IS_BIG_ENDIAN macro looks complicated, however all it does is
 * determine the endianess of the machine using trickery.
 *
 * See:
 * - https://stackoverflow.com/questions/2100331/c-macro-definition-to-determine-big-endian-or-little-endian-machine
 * - https://en.wikipedia.org/wiki/Endianness */
#define IS_BIG_ENDIAN       (!(union { uint16_t u16; unsigned char c; }){ .u16 = 1 }.c)

/**@brief When designing a binary format, which this interpreter uses and
 * saves to disk, it is imperative that certain information is saved to
 * disk - one of those pieces of information is the version of the
 * interpreter. Something such as this may seem trivial, but only once you
 * start to deploy applications to different machines and to different users
 * does it become apparent how important this is. */
#define CORE_VERSION        (0x02u) /**< version of the forth core file */

/* ============================ Section 2 ================================== */
/*                     Enumerations and Constants                            */

/**
 * This following string is a forth program that gets called when creating a
 * new Forth environment, it is not actually the very first program that gets
 * run, but it is run before the user gets a chance to do anything.
 *
 * The program is kept as small as possible, but is dependent on the virtual
 * machine image being set up correctly with a few other words being defined
 * first, they will be described as they are encountered. Suffice to say,
 * before this program is executed the following happens:
 *   1) The virtual machine image is initialized
 *   2) All the virtual machine primitives are defined
 *   3) All registers are named and some constants defined
 *   4) ";" is defined
 *
 * Of note, words such as "if", "else", "then", and even comments - "(" -, are
 * not actually Forth primitives, there are defined in terms of other Forth
 * words.
 *
 * The Forth interpreter is a very simple loop that does the following:
 *
 *   Start the interpreter loop <-----------<-----------------<---.
 *   Get a space delimited word                                    \
 *   Attempt to look up that word in the dictionary                 \
 *   Was the word found?                                             ^
 *   |-Yes:                                                          |
 *   |   Are we in compile mode?                                     |
 *   |   |-Yes:                                                      ^
 *   |   | \-Is the Word an Immediate word?                          |
 *   |   |   |-Yes:                                                  |
 *   |   |   | \-Execute the word >--------->----------------->----->.
 *   |   |   \-No:                                                   |
 *   |   |     \-Compile the word into the dictionary >------->----->.
 *   |   \-No:                                                       |
 *   |     \-Execute the word >------------->----------------->----->.
 *   \-No:                                                           ^
 *     \-Can the word be treated as a number?                        |
 *       |-Yes:                                                      |
 *       | \-Are we in compile mode?                                 |
 *       |   |-Yes:                                                  |
 *       |   | \-Compile a literal into the dictionary >------>----->.
 *       |   \-No:                                                   |
 *       |     \-Push the number to the variable stack >------>----->.
 *       \-No:                                                       |
 *         \-An Error has occurred, print out an error message >---->.
 *
 *  As you can see, there is not too much too it, however there are still a lot
 *  of details left out, such as how exactly the virtual machine executes words
 *  and how this loop is formed.
 *
 *  A short description of the words defined in "initial_forth_program"
 *  follows, bear in mind that they depend on the built in primitives, the
 *  named registers being defined, as well as "state" and ";".
 *
 *  here      - push the current dictionary pointer
 *  [         - immediately enter command mode
 *  ]         - enter compile mode
 *  >mark     - make a hole in the dictionary and push a pointer to it
 *  :noname   - make an anonymous word definition, push token to it, the
 *            definition is terminated by ';' like normal word defitions.
 *  if        - immediate word, begin if...else...then clause
 *  else      - immediate word, optional else clause
 *  then      - immediate word, end if...else...then clause
 *  begin     - immediate word, start a begin...until loop
 *  until     - immediate word, end begin...until loop, jump to matching
 *            begin at run time if top of stack is zero.
 *  '('       - push a ")" character to the stack
 *  (         - begin a Forth comment, terminated by a )
 *  rot       - perform stack manipulation: x y z => y z x
 *  -rot      - perform stack manipulation: x y z => z x y
 *  tuck      - perform stack manipulation: x y => y x y
 *  nip       - perform stack manipulation: x y => y
 *  allot     - allocate space in the dictionary
 *  bl        - push the space character to the stack
 *  space     - print a space
 *  .         - print out current top of stack, followed by a space **/
static const char *initial_forth_program = "                       \n\
: here h @ ;                                                       \n\
: [ immediate 0 state ! ;                                          \n\
: ] 1 state ! ;                                                    \n\
: >mark here 0 , ;                                                 \n\
: :noname immediate -1 , here 2 , ] ;                              \n\
: if immediate ' ?branch , >mark ;                                 \n\
: else immediate ' branch , >mark swap dup here swap - swap ! ;    \n\
: then immediate dup here swap - swap ! ;                          \n\
: begin immediate here ;                                           \n\
: until immediate ' ?branch , here - , ;                           \n\
: ')' 41 ;                                                         \n\
: ( immediate begin key ')' = until ; ( We can now use comments! ) \n\
: rot >r swap r> swap ;                                            \n\
: -rot rot rot ;                                                   \n\
: tuck swap over ;                                                 \n\
: nip swap drop ;                                                  \n\
: allot here + h ! ;                                               \n\
: bl 32 ;                                                          \n\
: emit _emit drop ;                                                \n\
: space bl emit ;                                                  \n\
: . pnum drop space ; ";

/**@brief This is a string used in number to string conversion in
 * number_printer, which is dependent on the current base*/
static const char conv[] = "0123456789abcdefghijklmnopqrstuvwxzy";

/**@brief These are the file access methods available for use when the virtual
 * machine is up and running, they are passed to the built in primitives that
 * deal with file input and output (such as open-file) */
static const char *fams[] = { "wb", "rb", "r+b", NULL };

/**@brief int to char* map for file access methods */
enum fams { 
	FAM_WO,   /**< write only */
	FAM_RO,   /**< read only */
	FAM_RW,   /**< read write */
	LAST_FAM  /**< marks last file access method */
};

/**@brief The following are different reactions errors can take when
 * using longjmp to a previous setjump*/
enum errors
{
	INITIALIZED, /**< setjmp returns zero if returning directly */
	OK,          /**< no error, do nothing */
	FATAL,       /**< fatal error, this invalidates the Forth image */
	RECOVERABLE, /**< recoverable error, this will reset the interpreter */
};

/**@brief An enumeration describing a save Forth images header
 *
 * We can serialize the Forth virtual machine image, saving it to disk so we
 * can load it again later. When saving the image to disk it is important
 * to be able to identify the file somehow, and to identify various
 * properties of the image.
 *
 * Unfortunately each image is not portable to machines with different
 * cell sizes (determined by "sizeof(forth_cell_t)") and different endianess,
 * and it is not trivial to convert them due to implementation details.
 *
 * "enum header" names all of the different fields in the header.
 *
 * The first four fields (MAGIC0...MAGIC3) are magic numbers which identify
 * the file format, so utilities like "file" on Unix systems can differentiate
 * binary formats from each other.
 *
 * CELL_SIZE is the size of the virtual machine cell used to create the image.
 *
 * VERSION is used to both represent the version of the Forth interpreter and
 * the version of the file format.
 *
 * ENDIAN is the endianess of the VM
 *
 * MAGIC7 is the last magic number.
 *
 * When loading the image the magic numbers are checked as well as
 * compatibility between the saved image and the compiled Forth interpreter. */
enum header {
	MAGIC0,     /**< Magic number used to identify file type */
	MAGIC1,     /**< Magic number ... */
	MAGIC2,     /**< Magic number ... */
	MAGIC3,     /**< Magic number ...  */
	CELL_SIZE,  /**< Size of a Forth cell, or virtual machine word */
	VERSION,    /**< Version of the image */
	ENDIAN,     /**< Endianess of the interpreter */
	MAGIC7      /**< Final magic number */
};

/* The header itself, this will be copied into the forth_t structure on
 * initialization, the ENDIAN field is filled in then as it seems impossible
 * to determine the endianess of the target at compile time. */
static const uint8_t header[MAGIC7+1] = {
	[MAGIC0]    = 0xFF,
	[MAGIC1]    = '4',
	[MAGIC2]    = 'T',
	[MAGIC3]    = 'H',
	[CELL_SIZE] = sizeof(forth_cell_t),
	[VERSION]   = CORE_VERSION,
	[ENDIAN]    = -1,
	[MAGIC7]    = 0xFF
};

/**@brief This is the main structure used by the virtual machine
 *
 * The structure is defined here and not in the header to hide the
 * implementation details it, all API functions are passed an opaque pointer to
 * the structure (see https://en.wikipedia.org/wiki/Opaque_pointer).
 *
 * Only three fields are serialized to the file saved to disk:
 *
 *   1) header
 *   2) core_size
 *   3) m
 *
 * And they are done so in that order, "core_size" and "m" are save in whatever
 * endianess the machine doing the saving is done in, however "core_size" is
 * converted to a "uint64_t" before being save to disk so it is not of a
 * variable size. "m" is a flexible array member "core_size" number of members.
 *
 * The "m" field is the virtual machines working memory, it has its own
 * internal structure which includes registers, stacks and a dictionary of
 * defined words.
 *
 * The "m" field is laid out as follows, assuming the size of the virtual
 * machine is 32768 cells big:
 *
 *       .-----------------------------------------------.
 *      | 0-3F      | 40-7BFF       |7C00-7DFF|7E00-7FFF|
 *      .-----------------------------------------------.
 *      | Registers | Dictionary... | V stack | R stack |
 *      .-----------------------------------------------.
 *
 *       V stack = The Variable Stack
 *       R stack = The Return Stack
 *
 * The dictionary has its own complex structure, and it always starts just
 * after the registers. It includes scratch areas for parsing words, start up
 * code and empty space yet to be consumed before the variable stack. The sizes
 * of the variable and returns stack change depending on the virtual machine
 * size. The structures within the dictionary will be described later on.*/
struct forth { /**< FORTH environment, values marked '~~' are serialized in order */
	uint8_t header[sizeof(header)]; /**< ~~ header for reloadable core file */
	forth_cell_t core_size;  /**< ~~ size of VM (converted to uint64_t for serialization) */
	uint8_t *s;          /**< convenience pointer for string input buffer */
	char hex_fmt[16];    /**< calculated hex format */
	char word_fmt[16];   /**< calculated word format */
	forth_cell_t *S;     /**< stack pointer */
	forth_cell_t *vstart;/**< index into m[] where the variable stack starts*/
	forth_cell_t *vend;  /**< index into m[] where the variable stack ends*/
	forth_cell_t m[];    /**< ~~ Forth Virtual Machine memory */
};

/**@brief actions to take on error, this does not override the behavior of the
 * virtual machine when it detects an error that cannot be recovered from, only
 * when it encounters an error such as a divide by zero or a word not being
 * found, not when the virtual machine executes and invalid instruction (which
 * should never normally happen unless something has been corrupted). */
enum actions_on_error
{
	ERROR_RECOVER,    /**< recover when an error happens, like a call to ABORT */
	ERROR_HALT,       /**< halt on error */
	ERROR_INVALIDATE, /**< halt on error and invalid the Forth interpreter */
};

/**@brief These are the possible options for the debug registers.*/
enum trace_level
{
	DEBUG_OFF,         /**< tracing is off */
	DEBUG_INSTRUCTION, /**< instructions and stack are traced */
	DEBUG_CHECKS       /**< bounds checks are printed out */
};

/**@brief A list of all the registers placed in the "m" field of "struct forth"
 *
 * There are a number of registers available to the virtual machine, they are
 * actually indexes into the virtual machines main memory, this is so that the
 * programs running on the virtual machine can access them. There are other
 * registers that are in use that the virtual machine cannot access directly
 * (such as the program counter or instruction pointer). Some of these
 * registers correspond directly to well known Forth concepts, such as the
 * dictionary and return stack pointers, others are just implementation
 * details. */
enum registers {          /**< virtual machine registers */
	DIC         =  6, /**< dictionary pointer */
	RSTK        =  7, /**< return stack pointer */
	STATE       =  8, /**< interpreter state; compile or command mode */
	BASE        =  9, /**< base conversion variable */
	PWD         = 10, /**< pointer to previous word */
	SOURCE_ID   = 11, /**< input source selector */
	SIN         = 12, /**< string input pointer */
	SIDX        = 13, /**< string input index */
	SLEN        = 14, /**< string input length */
	START_ADDR  = 15, /**< pointer to start of VM */
	FIN         = 16, /**< file input pointer */
	FOUT        = 17, /**< file output pointer */
	STDIN       = 18, /**< file pointer to stdin */
	STDOUT      = 19, /**< file pointer to stdout */
	STDERR      = 20, /**< file pointer to stderr */
	ARGC        = 21, /**< argument count */
	ARGV        = 22, /**< arguments */
	DEBUG       = 23, /**< turn debugging on/off if enabled */
	INVALID     = 24, /**< if non zero, this interpreter is invalid */
	TOP         = 25, /**< *stored* version of top of stack */
	INSTRUCTION = 26, /**< start up instruction */
	STACK_SIZE  = 27, /**< size of the stacks */
	ERROR_HANDLER = 28, /**< actions to take on error */
};

/** @brief enum input_stream contains the possible value of the SOURCE_ID
 *  register
 *
 *  Input in Forth systems traditionally (tradition is a word we will keep using
 *  here, generally in the context of programming it means justification for
 *  cruft) came from either one of two places, the keyboard that the programmer
 *  was typing at, interactively, or from some kind of non volatile store, such
 *  as a floppy disk. Our C program has no (easy and portable) way of
 *  interacting directly with the keyboard, instead it could interact with a
 *  file handle such as stdin, or read from a string. This is what we do in
 *  this interpreter.
 *
 *  A word in Forth called "SOURCE-ID" can be used to query what the input
 *  device currently is, the values expected are zero for interactive
 *  interpretation, or minus one (minus one, or all bits set, is used to
 *  represent truth conditions in most Forths, we are a bit more liberal in our
 *  definition of true) for string input. These are the possible values that
 *  the SOURCE_ID register can take.
 *
 *  Note that the meaning is slightly different in our Forth to what is meant
 *  traditionally, just because this program is taking input from stdin (or
 *  possibly another file handle), does not mean that this program is being
 *  run interactively, it could possibly be part of a Unix pipe. As such this
 *  interpreter defaults to being as silent as possible.
 *
 *  @todo SOURCE_ID needs extending with the semantics of File Access Words
 *  optional word set in DPANS94.
 */
enum input_stream {
	FILE_IN,       /**< file input; this could be interactive input */
	STRING_IN = -1 /**< string input */
};

/**@brief Instead of using numbers to refer to registers, it is better to refer to
 * them by name instead, these strings each correspond in turn to enumeration
 * called "registers" */
static const char *register_names[] = { "h", "r", "`state", "base", "pwd",
"`source-id", "`sin", "`sidx", "`slen", "`start-address", "`fin", "`fout", "`stdin",
"`stdout", "`stderr", "`argc", "`argv", "`debug", "`invalid", "`top", "`instruction",
"`stack-size", "`error-handler", NULL };

/** @brief enum for all virtual machine instructions
 *
 * "enum instructions" contains each virtual machine instruction, a valid
 * instruction is less than LAST. One of the core ideas of Forth is that
 * given a small set of primitives it is possible to build up a high level
 * language, given only these primitives it is possible to add conditional
 * statements, case statements, arrays and strings, even though they do not
 * exist as instructions here.
 *
 * Most of these instructions are quite simple (such as; pop two items off the
 * variable stack, add them and push the result for ADD) however others are a
 * great deal more complex and will take a few paragraphs to explain fully
 * (such as READ, or how IMMEDIATE interacts with the virtual machines
 * execution). */
enum instructions { PUSH,COMPILE,RUN,DEFINE,IMMEDIATE,READ,LOAD,STORE,CLOAD,CSTORE,
SUB,ADD,AND,OR,XOR,INV,SHL,SHR,MUL,DIV,ULESS,UMORE,SLESS,SMORE,EXIT,EMIT,KEY,FROMR,TOR,BRANCH,
QBRANCH, PNUM, QUOTE,COMMA,EQUAL,SWAP,DUP,DROP,OVER,TAIL,BSAVE,BLOAD,FIND,PRINT,
DEPTH,CLOCK,EVALUATE,PSTK,RESTART,SYSTEM,FCLOSE,FOPEN,FDELETE,FREAD,FWRITE,FPOS,
FSEEK,FFLUSH,FRENAME,LAST_INSTRUCTION };

/**@brief names of all named instructions, with a few exceptions
 *
 * So that we can compile programs we need ways of referring to the basic
 * programming constructs provided by the virtual machine, theses words are fed
 * into the C function "compile" in a process described later.
 *
 * LAST_INSTRUCTION is not an instruction, but only a marker of the last 
 * enumeration used in "enum instructions", so it does not get a name. */
static const char *instruction_names[] = { "push","compile","run","define", 
"immediate","read","@","!","c@","c!","-","+","and","or","xor","invert","lshift",
"rshift","*","/","u<","u>","<",">","exit","_emit","key","r>",">r","branch","?branch",
"pnum","'", ",","=", "swap","dup","drop", "over", "tail","bsave","bload",
"find","print","depth","clock","evaluate",".s","restart","system","close-file",
"open-file","delete-file","read-file","write-file","file-position","reposition-file",
"flush-file","rename-file", NULL };

/* ============================ Section 3 ================================== */
/*                  Helping Functions For The Compiler                       */

/**@brief  get a char from string input or a file
 * @param  o   forth image containing information about current input stream
 * @return int same value as fgetc or getchar
 *
 * This Forth interpreter only has a few mechanisms for I/O, one of these is
 * to fetch an individual character of input from either a string or a file
 * which can be set either with knowledge of the implementation from within
 * the virtual machine, or via the API presented to the programmer. The C
 * functions "forth_init", "forth_set_file_input" and
 * "forth_set_string_input" set up and manipulate the input of the
 * interpreter. These functions act on the following registers:
 *
 * SOURCE_ID - The current input source (SIN or FIN)
 * SIN       - String INput
 * SIDX      - String InDeX
 * SLEN      - String LENgth
 * FIN       - File   INput
 *
 * Note that either SIN or FIN might not both be valid, one will be but the
 * other might not, this makes manipulating these values hazardous. The input
 * functions "forth_get_char" and "forth_get_word" both take their input
 * streams implicitly via the registers contained within the Forth execution
 * environment passed in to those functions.*/
static int forth_get_char(forth_t *o)
{
	switch(o->m[SOURCE_ID]) {
	case FILE_IN:   return fgetc((FILE*)(o->m[FIN]));
	case STRING_IN: return o->m[SIDX] >= o->m[SLEN] ? EOF : ((char*)(o->m[SIN]))[o->m[SIDX]++];
	default:        return EOF;
	}
}

/**@brief get a word (space delimited, up to 31 chars) from a FILE* or string-in
 * @param o    initialized Forth environment.
 * @param p    pointer to string to write into
 * @return int return status of [fs]scanf
 *
 * This function reads in a space delimited word, limited to MAXIMUM_WORD_LENGTH,
 * the word is put into the pointer "*p", due to the simple nature of Forth
 * this is as complex as parsing and lexing gets. It can either read from
 * a file handle or a string, like forth_get_char() */
static int forth_get_word(forth_t *o, uint8_t *p)
{
	int n = 0;
	switch(o->m[SOURCE_ID]) {
	case FILE_IN:   return fscanf((FILE*)(o->m[FIN]), o->word_fmt, p, &n);
	case STRING_IN:
		if(sscanf((char *)&(((char*)(o->m[SIN]))[o->m[SIDX]]), o->word_fmt, p, &n) < 0)
			return EOF;
		return o->m[SIDX] += n, n;
	default:       return EOF;
	}
}

/** @brief compile a Forth word header into the dictionary
 *  @param o    Forth environment to do the compilation in
 *  @param code virtual machine instruction for that word
 *  @param str  name of Forth word
 *
 *  The function "compile" is not that complicated in itself, however it
 *  requires an understanding of the structure of a Forth word definition and
 *  the behavior of the Forth run time.
 *
 *  In all Forth implementations there exists a concept of "the dictionary",
 *  although they may be implemented in different ways the usual way is as a
 *  linked list of words, starting with the latest defined word and ending with
 *  a special terminating value. Words cannot be arbitrarily deleted, and
 *  operation is largely append only. Each word or Forth function that has been
 *  defined can be looked up in this dictionary, and dependent on whether it is
 *  an immediate word or a compiling word, and whether we are in command or
 *  compile mode different actions are taken when we have found the word we are
 *  looking for in our Read-Evaluate-Loop.
 *
 *  | <-- Start of VM memory
 *  |                 | <-- Start of dictionary
 *                    |
 *  .------------.    |  .------.      .------.             .-------------.
 *  | Terminator | <---- | Word | <--- | Word | < -- ... -- | Latest Word |
 *  .------------.    |  .------.      .------.             .-------------.
 *                    |                                          ^
 *                    |                                          |
 *                    |                                      PWD Register
 *
 *  The PWD registers points to the latest defined word, a search starts from
 *  here and works it way backwards (allowing us replace old definitions by
 *  appending new ones with the same name only), the terminator
 *
 *  Our word header looks like this:
 *
 *  .-----------.-----.------.--------.------------.
 *  | Word Name | PWD | MISC | CODE-2 | Data Field |
 *  .-----------.-----.------.--------.------------.
 *
 *   - "CODE-2" and the "Data Field" are optional and the "Data Field" is of
 *   variable length.
 *   - "Word Name" is a variable length field whose size is recorded in the
 *     MISC field.
 *
 *   And the MISC field is a composite to save space containing a virtual
 *   machine instruction, the hidden bit and the length of the Word Name string
 *   as an offset in cells from PWD field. The field looks like this:
 *
 *   -----.-------------------.------------.-------------.
 *    ... | 16 ........... 8  |    9       | 7 ....... 0 |
 *    ... |  Word Name Size   | Hidden Bit | Instruction |
 *   -----.-------------------.------------.-------------.
 *
 *   The maximum value for the Word Name field is determined by the Word Name
 *   Size field and a few other constants in the interpreter.
 *
 *   The hidden bit is not used in the "compile" function, but is used
 *   elsewhere (in forth_find) to hide a word definition from the word
 *   search. The hidden bit is not set within this program at all, however it
 *   can be set by a running Forth virtual machine (and it is, if desired).
 *
 *   The "Instruction" tells the interpreter what to do with the Word
 *   definition when it is found and how to interpret "CODE-2" and the
 *   "Data Field" if they exist. */
static void compile(forth_t *o, forth_cell_t code, const char *str)
{ /* create a new forth word header */
	assert(o && code < LAST_INSTRUCTION);
	forth_cell_t *m = o->m, header = m[DIC], l = 0;
	/*FORTH header structure */
	strcpy((char *)(o->m + header), str); /* 0: Copy the new FORTH word into the new header */
	l = strlen(str) + 1;
	l = (l + (sizeof(forth_cell_t) - 1)) & ~(sizeof(forth_cell_t) - 1); /* align up to sizeof word */
	l = l/sizeof(forth_cell_t);
	m[DIC] += l; /* Add string length in words to header (STRLEN) */

	m[m[DIC]++] = m[PWD];     /*0 + STRLEN: Pointer to previous words header */
	m[PWD] = m[DIC] - 1;      /*   Update the PWD register to new word */
	m[m[DIC]++] = (l << WORD_LENGTH_OFFSET) | code; /*1: size of words name and code field */
}

/** @brief implement the Forth block I/O mechanism
 *  @param o       virtual machine image to do the block I/O in
 *  @param poffset offset into o->m field to load or save
 *  @param id      Identification of block to read or write
 *  @param rw      Mode of operation 'r' == read, 'w' == write
 *  @return negative number on failure, zero on success
 *
 * Forth traditionally uses blocks as its method of storing data and code to
 * disk, each block is BLOCK_SIZE characters long (which should be 1024
 * characters). The reason for such a simple method is that many early Forth
 * systems ran on microcomputers which did not have an operating system as
 * they are now known, but only a simple monitor program and a programming
 * language, as such there was no file system either. Each block was loaded
 * from disk and then evaluated.
 *
 * The "blockio" function implements this simple type of interface, and can
 * load and save blocks to disk. */
static int blockio(forth_t *o, forth_cell_t poffset, forth_cell_t id, char rw)
{ /* simple block I/O, could be replaced with making fopen/fclose available to interpreter */
	char name[16] = {0}; /* XXXX + ".blk" + '\0' + a little spare change */
	FILE *file = NULL;
	size_t n;
	if(((forth_cell_t)poffset) > ((o->core_size * sizeof(forth_cell_t)) - BLOCK_SIZE))
		return -1;
	sprintf(name, "%04x.blk", (int)id);
	if(!(file = fopen(name, rw == 'r' ? "rb" : "wb"))) { /**@todo loading should always succeed */
		fprintf(stderr, "( error 'file-open \"%s : could not open file\" )\n", name);
		return -1;
	}
	n = rw == 'w' ? fwrite(((char*)o->m) + poffset, 1, BLOCK_SIZE, file):
			fread (((char*)o->m) + poffset, 1, BLOCK_SIZE, file);
	fclose(file);
	return n == BLOCK_SIZE ? 0 : -1;
}

/**@brief turn a string into a number using a base and return an error code to
 * indicate success or failure, the results of the conversion are stored in n,
 * even if the conversion failed.
 * @param  base base to convert string from, valid values are 0, and 2-26
 * @param  n    out parameter, the result of the conversion is stored here
 * @param  s    string to convert
 * @return int return code indicating failure or success */
static int numberify(int base, forth_cell_t *n, const char *s)
{
	char *end = NULL;
	errno = 0;
	*n = strtol(s, &end, base);
	return !errno && *s != '\0' && *end == '\0';
}

/** @brief case insensitive string comparison
 *  @param  a   first string to compare
 *  @param  b   second string
 *  @return int zero if both strings are the same, non zero otherwise (same as
 *  strcmp, only insensitive to case)
 *
 *  Forths are usually case insensitive and are required to be (or at least
 *  accept only uppercase characters only) by many of the standards for Forth.
 *  As an aside I do not believe case insensitivity is a good idea as it
 *  complicates interfaces and creates as much confusion as it tries to solve
 *  (not only that different case letters do convey information). However,
 *  in keeping with other implementations, this Forth is also made insensitive
 *  to case "DUP" is treated the same as "dup" and "Dup".
 *
 *  This comparison function, istrcmp, is only used in one place however, in
 *  the C function "forth_find", replacing it with "strcmp" will bring back the
 *  more logical, case sensitive, behavior. */
static int istrcmp(const uint8_t *a, const uint8_t *b)
{
	for(; ((*a == *b) || (tolower(*a) == tolower(*b))) && *a && *b; a++, b++)
		;
	return tolower(*a) - tolower(*b);
}

/* "forth_find" finds a word in the dictionary and if it exists it returns a
 * pointer to its PWD field. If it is not found it will return zero, also of
 * notes is the fact that it will skip words that are hidden, that is the
 * hidden bit in the MISC field of a word is set. The structure of the
 * dictionary has already been explained, so there should be no surprises in
 * this word. Any improvements to the speed of this word would speed up the
 * interpreter a lot. */
forth_cell_t forth_find(forth_t *o, const char *s)
{
	forth_cell_t *m = o->m, w = m[PWD], len = WORD_LENGTH(m[w+1]);
	for (;w > DICTIONARY_START && (WORD_HIDDEN(m[w+1]) || istrcmp((uint8_t*)s,(uint8_t*)(&o->m[w - len])));) {
		w = m[w];
		len = WORD_LENGTH(m[w+1]);
	}
	return w > DICTIONARY_START ? w+1 : 0;
}

/**@brief Print a number in a given base to an output stream
 * @param u    number to print
 * @param base base to print in (must be between 1 and 37)
 * @param out  output file stream
 * @return zero or positive on success, negative on failure */
static int print_unsigned_number(forth_cell_t u, forth_cell_t base, FILE *out)
{
	assert(base > 1 && base < 37);
	int i = 0, r = 0;
	char s[64 + 1] = ""; 
	do 
		s[i++] = conv[u % base];
	while ((u /= base));
	for(; i >= 0 && r >= 0; i--)
		r = fputc(s[i], out);
	return r;
}

/**@brief  print out a forth cell as a number, the output base being determined
 *         by the BASE registers
 * @param  o     an initialized forth environment (contains BASE register and
 *               output streams)
 * @param  f     value to print out
 * @return int   zero or positive on success, negative on failure */
static int print_cell(forth_t *o, FILE *output, forth_cell_t f)
{
	unsigned base = o->m[BASE];
	if(base == 10 || base == 0)
		return fprintf(output, "%"PRIdCell, f);
	if(base == 16)
		return fprintf(output, o->hex_fmt, f);
	if(base == 1 || base > 36)
		return -1;
	return print_unsigned_number(f, base, output);
}

static void debug_checks(forth_t *o, forth_cell_t f, unsigned line)
{
	(void)o;
	fprintf(stderr, "\t( debug\t0x%" PRIxCell "\t%u )\n", f, line);
}

/* "check_bounds" is used to both check that a memory access performed by the
 * virtual machine is within range and as a crude method of debugging the
 * interpreter (if it is enabled). The function is not called directly but
 * is instead wrapped in with the "ck" macro, it can be removed completely
 * with compile time defines, removing the check and the debugging code. */
static forth_cell_t check_bounds(forth_t *o, jmp_buf *on_error, forth_cell_t f, unsigned line, forth_cell_t bound)
{
	if(o->m[DEBUG] >= DEBUG_CHECKS)
		debug_checks(o, f, line);
	if(f >= bound) {
		fprintf(stderr, "( fatal \" bounds check failed: %" PRIdCell " >= %zu\" )\n", f, (size_t)bound);
		longjmp(*on_error, FATAL);
	}
	return f;
}

/* "check_depth" is used to check that there are enough values on the stack
 * before an operation takes place. It is wrapped up in the "cd" macro. */
static void check_depth(forth_t *o, jmp_buf *on_error, forth_cell_t *S, forth_cell_t expected, unsigned line)
{
	if(o->m[DEBUG] >= DEBUG_CHECKS)
		debug_checks(o, (forth_cell_t)(S - o->vstart), line);
	if((uintptr_t)(S - o->vstart) < expected) {
		fprintf(stderr, "( error \" stack underflow\" )\n");
		longjmp(*on_error, RECOVERABLE);
	} else if(S > o->vend) {
		fprintf(stderr, "( error \" stack overflow\" )\n");
		longjmp(*on_error, RECOVERABLE);
	}
}

/* This checks that a Forth string is NUL terminated, as required by most C
 * functions, which should be the last character in string (which is s+end).
 * There is a bit of a mismatch between Forth strings (which are pointer
 * to the string and a length) and C strings, which a pointer to the string and
 * are NUL terminated. This function helps to correct that. */
static void check_is_asciiz(jmp_buf *on_error, char *s, forth_cell_t end)
{
	if(*(s + end) != '\0') {
		fprintf(stderr, "( error \" Not an ASCIIZ string\")\n");
		longjmp(*on_error, RECOVERABLE);
	}
}

/* This function gets a string off the Forth stack, checking that the string is
 * NUL terminated. It is a helper function used when a Forth string has to be
 * converted to a C string so it can be passed to a C function. */
static char *forth_get_string(forth_t *o, jmp_buf *on_error, forth_cell_t **S, forth_cell_t f)
{
	forth_cell_t length = f;
	char *string = ((char*)o->m) + **S;
	(*S)--;
	check_is_asciiz(on_error, string, length);
	return string;
}

/* Forth file access methods ("fam") must be held in a single cell, this
 * requires a method of translation from this cell into a string that can be
 * used by the C function "fopen" */
static const char* forth_get_fam(jmp_buf *on_error, forth_cell_t f)
{
	if(f >= LAST_FAM) {
		fprintf(stderr, "( error \" Invalid file access method\" %"PRIdCell")\n", f);
		longjmp(*on_error, RECOVERABLE);
	}
	return fams[f];
}

/* This prints out the Forth stack, which is useful for debugging. */
static void print_stack(forth_t *o, FILE *output, forth_cell_t *S, forth_cell_t f)
{ 
	forth_cell_t depth = (forth_cell_t)(S - o->vstart);
	fprintf(output, "%"PRIdCell": ", depth);
	if(!depth)
		return;
	print_cell(o, output, f);
	fputc(' ', output);
	while(o->vstart + 1 < S) {
		print_cell(o, output, *(S--));
		fputc(' ', output);
	}
	fputc('\n', output);
}

/* This function allows for some more detailed tracing to take place */
static void trace(forth_t *o, forth_cell_t instruction, forth_cell_t *S, forth_cell_t f)
{
	if(o->m[DEBUG] < DEBUG_INSTRUCTION)
		return;
	if(instruction > LAST_INSTRUCTION) {
		fprintf(stderr, "traced invalid instruction!\n");
		return;
	}
	fprintf(stderr, "\t( %s\t ", instruction_names[instruction]);
	print_stack(o, stderr, S, f);
	fputs(" )\n", stderr);
}

/* ============================ Section 4 ================================== */
/*              API related functions and Initialization code                */

void forth_set_file_input(forth_t *o, FILE *in)
{
	assert(o && in);
	o->m[SOURCE_ID] = FILE_IN;
	o->m[FIN]       = (forth_cell_t)in;
}

void forth_set_file_output(forth_t *o, FILE *out)
{
	assert(o && out);
	o->m[FOUT] = (forth_cell_t)out;
}

void forth_set_string_input(forth_t *o, const char *s)
{
	assert(o && s);
	o->m[SIDX] = 0;              /* m[SIDX] == current character in string */
	o->m[SLEN] = strlen(s) + 1;  /* m[SLEN] == string len */
	o->m[SOURCE_ID] = STRING_IN; /* read from string, not a file handle */
	o->m[SIN] = (forth_cell_t)s; /* sin  == pointer to string input */
}

int forth_eval(forth_t *o, const char *s)
{
	assert(o && s);
	forth_set_string_input(o, s);
	return forth_run(o);
}

int forth_define_constant(forth_t *o, const char *name, forth_cell_t c)
{
	char e[MAXIMUM_WORD_LENGTH+32] = {0};
	assert(o && strlen(name) < MAXIMUM_WORD_LENGTH);
	sprintf(e, ": %31s %" PRIdCell " ; \n", name, c);
	return forth_eval(o, e);
}

/** @brief default all the registers of a forth environment
 *  @param o     the forth environment to set up
 *  @param size  the size of the "m" field in "o"
 *  @param in    the input file
 *  @param out   the output file
 *
 *  "forth_make_default" default is called by "forth_init" and
 *  "forth_load_core", it is a routine which deals that sets up registers for
 *  the virtual machines memory, and especially with values that may only be
 *  valid for a limited period (such as pointers to stdin). */
static void forth_make_default(forth_t *o, size_t size, FILE *in, FILE *out)
{
	o->core_size     = size;
	o->m[STACK_SIZE] = size / MINIMUM_STACK_SIZE > MINIMUM_STACK_SIZE ?
				size / MINIMUM_STACK_SIZE :
				MINIMUM_STACK_SIZE;
	o->s             = (uint8_t*)(o->m + STRING_OFFSET); /* string store offset into CORE, skip registers */
	o->m[FOUT]       = (forth_cell_t)out;
	o->m[START_ADDR] = (forth_cell_t)&(o->m);
	o->m[STDIN]      = (forth_cell_t)stdin;
	o->m[STDOUT]     = (forth_cell_t)stdout;
	o->m[STDERR]     = (forth_cell_t)stderr;
	o->m[RSTK]       = size - o->m[STACK_SIZE];     /* set up return stk pointer */
	o->m[ARGC] = o->m[ARGV] = 0;
	o->S             = o->m + size - (2 * o->m[STACK_SIZE]); /* set up variable stk pointer */
	o->vstart        = o->m + size - (2 * o->m[STACK_SIZE]);
	o->vend          = o->vstart + o->m[STACK_SIZE];
	sprintf(o->hex_fmt, "0x%%0%d"PRIxCell, (int)sizeof(forth_cell_t)*2);
	sprintf(o->word_fmt, "%%%ds%%n", MAXIMUM_WORD_LENGTH - 1);
	forth_set_file_input(o, in);  /* set up input after our eval */
}

/**@brief This function simply copies the current Forth header into a byte
 * array, filling in the endianess which can only be determined at run time
 * @param dst a byte array at least "sizeof header" large */
static void make_header(uint8_t *dst)
{
	memcpy(dst, header, sizeof header);
	/*fill in endianess, needs to be done at run time */
	dst[ENDIAN] = !IS_BIG_ENDIAN;
}

/* "forth_init" is a complex function that returns a fully initialized forth
 * environment we can start executing Forth in, it does the usual task of
 * allocating memory for the object to be returned, but it also does has the
 * task of getting the object into a runnable state so we can pass it to
 * "forth_run" and do useful work. */
forth_t *forth_init(size_t size, FILE *in, FILE *out)
{
	assert(in && out);
	forth_cell_t *m, i, w, t;
	forth_t *o;
	assert(sizeof(forth_cell_t) >= sizeof(uintptr_t));
	assert(sizeof(forth_cell_t) == sizeof(forth_signed_cell_t));
	/*There is a minimum requirement on the "m" field in the "forth_t"
	* structure which is not apparent in its definition (and cannot be
	* made apparent given how flexible array members work). We need enough
	* memory to store the registers (32 cells), the parse area for a word
	* (MAXIMUM_WORD_LENGTH cells), the initial start up program (about 6
	* cells), the initial built in and defined word set (about 600-700 cells)
	* and the variable and return stacks (MINIMUM_STACK_SIZE cells each,
	* as minimum).
	*
	* If we add these together we come up with an absolute
	* minimum, although that would not allow us define new words or do
	* anything useful. We use MINIMUM_STACK_SIZE to define a useful
	* minimum, albeit a restricted on, it is not a minimum large enough to
	* store all the definitions in "forth.fth" (a file within the project
	* containing a lot of Forth code) but it is large enough for embedded
	* systems, for testing the interpreter and for the unit tests within
	* the "unit.c" file.
	*
	* We VERIFY that the size has been passed in is equal to or about
	* minimum as this has been documented as being a requirement to this
	* function in the C API, if we are passed a lower number the
	* programmer has made a mistake somewhere and should be informed of
	* this problem. */
	VERIFY(size >= MINIMUM_CORE_SIZE);
	if(!(o = calloc(1, sizeof(*o) + sizeof(forth_cell_t)*size)))
		return NULL;

	/* default the registers, and input and output streams */
	forth_make_default(o, size, in, out);

	/* o->header needs setting up, but has no effect on the run time
	 * behavior of the interpreter */
	make_header(o->header);

	m = o->m;       /*a local variable only for convenience */

	/*The next section creates a word that calls READ, then TAIL,
	* then itself. This is what the virtual machine will run at startup so
	* that we can start reading in and executing Forth code. It creates a
	* word that looks like this:
	*
	*    | <-- start of dictionary          |
	*    .------.------.-----.----.----.----.
	*    | TAIL | READ | RUN | P1 | P2 | P2 | Rest of dictionary ...
	*    .------.------.-----.----.----.----.
	*    |     end of this special word --> |
	*
	*    P1 is a pointer to READ
	*    P2 is a pointer to TAIL
	*    P2 is a pointer to RUN
	*
	* The effect of this can be described as "make a function which
	* performs a READ then calls itself tail recursively". The first
	* instruction run is "RUN" which we save in "o->m[INSTRUCTION]" and
	* restore when we enter "forth_run".
	*
	 */
	o->m[PWD]   = 0;  /*special terminating pwd value */
	t = m[DIC] = DICTIONARY_START; /*initial dictionary offset, skip registers and string offset */
	m[m[DIC]++] = TAIL; /*Add a TAIL instruction that can be called */
	w = m[DIC];         /*Save current offset, it will contain the READ instruction */
	m[m[DIC]++] = READ; /*create a special word that reads in FORTH */
	m[m[DIC]++] = RUN;  /*call the special word recursively */
	o->m[INSTRUCTION] = m[DIC]; /*instruction stream points to our special word */
	m[m[DIC]++] = w;    /*call to READ word */
	m[m[DIC]++] = t;    /*call to TAIL */
	m[m[DIC]++] = o->m[INSTRUCTION] - 1; /* recurse */

	/*DEFINE and IMMEDIATE are two immediate words, the only two immediate
	* words that are also virtual machine instructions, we can make them
	* immediate by passing in their code word to "compile". The created
	* word looks like this
	*
	* .------.-----.------.
	* | NAME | PWD | MISC |
	* .------.-----.------.
	*
	* The MISC field here contains either DEFINE or IMMEDIATE, as well as
	* the hidden bit field and an offset to the beginning of name. */
	compile(o, DEFINE,    ":");
	compile(o, IMMEDIATE, "immediate");

	/*All of the other built in words that use a virtual machine
	* instruction to do work are instead compiling words, and because
	* there are lots of them we can initialize them in a loop
	*
	* The created word looks like this:
	*
	* .------.-----.------.----------------.
	* | NAME | PWD | MISC | VM-INSTRUCTION |
	* .------.-----.------.----------------.
	*
	* The MISC field here contains the COMPILE instructions, which will
	* compile a pointer to the VM-INSTRUCTION, as well as the other fields
	* it usually contains. */
	for(i = READ, w = READ; instruction_names[i]; i++) {
		compile(o, COMPILE, instruction_names[i]);
		m[m[DIC]++] = w++; /*This adds the actual VM instruction */
	}
	/*The next eval is the absolute minimum needed for a sane environment,
	* it defines two words "state" and ";" */
	VERIFY(forth_eval(o, ": state 8 exit : ; immediate ' exit , 0 state ! ;") >= 0);

	/*We now name all the registers so we can refer to them by name
	* instead of by number, this is not strictly necessary but is good
	* practice */
	for(i = 0; register_names[i]; i++)
		VERIFY(forth_define_constant(o, register_names[i], i+DIC) >= 0);

	/*Now we finally are in a state to load the slightly inaccurately
	* named "initial_forth_program", which will give us basic looping and
	* conditional constructs */
	VERIFY(forth_eval(o, initial_forth_program) >= 0);

	/*A few more constants are now defined, nothing special here */
	VERIFY(forth_define_constant(o, "size",        sizeof(forth_cell_t)) >= 0);
	VERIFY(forth_define_constant(o, "stack-start", size - (2 * o->m[STACK_SIZE])) >= 0);
	VERIFY(forth_define_constant(o, "max-core",    size) >= 0);
	VERIFY(forth_define_constant(o, "r/o",         FAM_RO) >= 0);
	VERIFY(forth_define_constant(o, "w/o",         FAM_WO) >= 0);
	VERIFY(forth_define_constant(o, "r/w",         FAM_RW) >= 0);

	/*All of the calls to "forth_eval" and "forth_define_constant" have
	* set the input streams to point to a string, we need to reset them
	* to they point to the file "in" */
	forth_set_file_input(o, in);  /*set up input after our eval */
	return o;
}

/* This is a crude method that should only be used for debugging purposes, it
 * simply dumps the forth structure to disk, including any padding which the
 * compiler might have inserted. This dump cannot be reloaded */
int forth_dump_core(forth_t *o, FILE *dump)
{
	assert(o && dump);
	size_t w = sizeof(*o) + sizeof(forth_cell_t) * o->core_size;
	return w != fwrite(o, 1, w, dump) ? -1: 0;
}

/* We can save the virtual machines working memory in a way, called
 * serialization, such that we can load the saved file back in and continue
 * execution using this save environment. Only the three previously mentioned
 * fields are serialized; "m", "core_size" and the "header". */
int forth_save_core(forth_t *o, FILE *dump)
{
	assert(o && dump);
	uint64_t r1, r2, r3, core_size = o->core_size;
	r1 = fwrite(o->header,  1, sizeof(o->header), dump);
	r2 = fwrite(&core_size, sizeof(core_size), 1, dump);
	r3 = fwrite(o->m,       1, sizeof(forth_cell_t) * o->core_size, dump);
	if(r1 + r2 + r3 != (sizeof(o->header) + 1 + sizeof(forth_cell_t) * o->core_size))
		return -1;
	return 0;
}

/* Logically if we can save the core for future reuse, then we must have a
 * function for loading the core back in, this function returns a reinitialized
 * Forth object. Validation on the object is performed to make sure that it is
 * a valid object and not some other random file, endianess, core_size, cell
 * size and the headers magic constants field are all checked to make sure they
 * are correct and compatible with this interpreter.
 *
 * "forth_make_default" is called to replace any instances of pointers stored
 * in registers which are now invalid after we have loaded the file from disk. */
forth_t *forth_load_core(FILE *dump)
{ /* load a forth core dump for execution */
	uint8_t actual[sizeof(header)] = {0}, expected[sizeof(header)] = {0};
	forth_t *o = NULL;
	uint64_t w = 0, core_size = 0;
	make_header(expected);
	if(sizeof(actual) != fread(actual, 1, sizeof(actual), dump))
		goto fail; /* no header */
	if(memcmp(expected, actual, sizeof(header)))
		goto fail; /* invalid or incompatible header */
	if(1 != fread(&core_size, sizeof(core_size), 1, dump) || core_size < MINIMUM_CORE_SIZE)
		goto fail; /* no header, or size too small */
	w = sizeof(*o) + (sizeof(forth_cell_t) * core_size);
	if(!(o = calloc(w, 1)))
		goto fail; /* object too big */
	w = sizeof(forth_cell_t) * core_size;
	/**@todo succeed if o->m[DIC] bytes read in?*/
	if(w != fread(o->m, 1, w, dump))
		goto fail; /* not enough bytes in file */
	o->core_size = core_size;
	memcpy(o->header, actual, sizeof(o->header));
	forth_make_default(o, core_size, stdin, stdout);
	return o;
fail:
	free(o);
	return NULL;
}

/* free the Forth interpreter we make sure to invalidate the interpreter
 * in case there is a use after free */
void forth_free(forth_t *o)
{
	assert(o);
	o->m[INVALID] = 1; /* a sufficiently "smart" compiler might optimize this out */
	free(o);
}

/* "forth_push", "forth_pop" and "forth_stack_position" are the main ways an
 * application programmer can interact with the Forth interpreter. Usually this
 * tutorial talks about how the interpreter and virtual machine work, about how
 * compilation and command modes work, and the internals of a Forth
 * implementation. However this project does not just present an ordinary Forth
 * interpreter, the interpreter can be embedded into other applications, and it
 * is possible be running multiple instances Forth interpreters in the same
 * process.
 *
 * The project provides an API which other programmers can use to do this, one
 * mechanism that needs to be provided is the ability to move data into and out
 * of the interpreter, these C level functions are how this mechanism is
 * achieved. They move data between a C program and a paused Forth interpreters
 * variable stack. */

void forth_push(forth_t *o, forth_cell_t f)
{
	assert(o && o->S < o->m + o->core_size);
	*++(o->S) = o->m[TOP];
	o->m[TOP] = f;
}

forth_cell_t forth_pop(forth_t *o)
{
	assert(o && o->S > o->m);
	forth_cell_t f = o->m[TOP];
	o->m[TOP] = *(o->S)--;
	return f;
}

forth_cell_t forth_stack_position(forth_t *o)
{
	assert(o);
	return o->S - o->vstart;
}

/* ============================ Section 5 ================================== */
/*                      The Forth Virtual Machine                            */

/* The largest function in the file, which implements the forth virtual
 * machine, everything else in this file is just fluff and support for this
 * function. This is the Forth virtual machine, it implements a threaded
 * code interpreter (see https://en.wikipedia.org/wiki/Threaded_code, and
 * https://www.complang.tuwien.ac.at/forth/threaded-code.html). */
int forth_run(forth_t *o)
{
	int errorval = 0;
	assert(o);
	jmp_buf on_error;
	if(o->m[INVALID]) {
		fprintf(stderr, "fatal: refusing to run an invalid forth\n");
		return -1;
	}

	/* The following code handles errors, if an error occurs, the
	 * interpreter will jump back to here.
	 *
	 * @todo This code needs to be rethought to be made more compliant with
	 * how "throw" and "catch" work in Forth. */
	if ((errorval = setjmp(on_error)) || o->m[INVALID]) {
		/* If the interpreter gets into an invalid state we always
		 * exit, which */
		if(o->m[INVALID])
			return -1;
		switch(errorval) {
			default:
			case FATAL:
				return -(o->m[INVALID] = 1);
			/* recoverable errors depend on o->m[ERROR_HANDLER],
			 * a register which can be set within the running
			 * virtual machine. */
			case RECOVERABLE:
				switch(o->m[ERROR_HANDLER]) {
				case ERROR_INVALIDATE: o->m[INVALID] = 1;
				case ERROR_HALT:       return -(o->m[INVALID]);
				case ERROR_RECOVER:    o->m[RSTK] = o->core_size - o->m[STACK_SIZE];
						       break;
				}
			case OK: break;
		}
	}
	
	forth_cell_t *m = o->m, pc, *S = o->S, I = o->m[INSTRUCTION], f = o->m[TOP], w, clk;

	clk = (1000 * clock()) / CLOCKS_PER_SEC;

	/*The following section will explain how the threaded virtual machine
	* interpreter works. Threaded code is quite a simple concept and
	* Forths typically compile their code to threaded code, it suites
	* Forth implementations as word definitions consist of juxtaposition
	* of previously defined words until they reach a set of primitives.
	*
	* This means a function like "square" will be implemented like this:
	*
	*   call dup   <- duplicate the top item on the variable stack
	*   call *     <- push the result of multiplying the top two items
	*   call exit  <- exit the definition of square
	*
	* Each word definition is like this, a series of calls to other
	* functions. We can optimize this by removing the explicit "call" and
	* just having a series of code address to jump to, which will become:
	*
	*   address of "dup"
	*   address of "*"
	*   address of "exit"
	*
	* We now have the problem that we cannot just jump to the beginning of
	* the definition of "square" in our virtual machine, we instead use
	* an instruction (RUN in our interpreter, or DOLIST as it is sometimes
	* known in most other implementations) to determine what to do with the
	* following data, if there is any. This system also allows us to encode
	* primitives, or virtual machine instructions, in the same way as we
	* encode words. If our word does not have the RUN instruction as its
	* first instruction then the list of addresses will not be interpreted
	* but only a simple instruction will be executed.
	*
	* The for loop and the switch statement here form the basis of our
	* thread code interpreter along with the program counter register (pc)
	* and the instruction pointer register (I).
	*
	* To explain how execution proceeds it will help to refer to the
	* internal structure of a word and how words are compiled into the
	* dictionary.
	*
	* Above we saw that a words layout looked like this:
	*
	*    .-----------.-----.------.--------.------------.
	*    | Word Name | PWD | MISC | CODE-2 | Data Field |
	*    .-----------.-----.------.--------.------------.
	*
	* During execution we do not care about the "Word Name" field and
	* "PWD" field. Also during execution we do not care about the top bits
	* of the MISC field, only what instruction it contains.
	*
	* Immediate words looks like this:
	*
	*   .-------------.---------------------.
	*   | Instruction | Optional Data Field |
	*   .-------------.---------------------.
	*
	* And compiling words look like this:
	*
	*   .---------.-------------.---------------------.
	*   | COMPILE | Instruction | Optional Data Field |
	*   .---------.-------------.---------------------.
	*
	* If the data field exists, the "Instruction" field will contain
	* "RUN". For words that only implement a single virtual machine
	* instruction the "Instruction" field will contain only that single
	* instruction (such as ADD, or SUB).
	*
	* Let us define a series of words and see how the resulting word
	* definitions are laid out, discounting the "Word Name", "PWD" and the
	* top bits of the "MISC" field.
	*
	* We will define two words "square" (which takes a number off the
	* stack, multiplies it by itself and pushes the result onto the stack) and
	* "sum-of-products" (which takes two numbers off the stack, squares
	* each one, adds the two results together and pushes the result onto
	* the stack):
	*
	*    : square           dup * ;
	*    : sum-of-products  square swap square + ;
	*
	* Executing these:
	*
	*  9 square .            => prints '81 '
	*  3 4 sum-of-products . => prints '25 '
	*
	* "square" refers to two built in words "dup" and "*",
	* "sum-of-products" to the word we just defined and two built in words
	* "swap" and "+". We have also used the immediate word ":" and ";".
	*
	* Definition of "dup", a compiling word:
	* .---------.------.
	* | COMPILE | DUP  |
	* .---------.------.
	* Definition of "+", a compiling word:
	* .---------.------.
	* | COMPILE | +    |
	* .---------.------.
	* Definition of "swap", a compiling word:
	* .---------.------.
	* | COMPILE | SWAP |
	* .---------.------.
	* Definition of "exit", a compiling word:
	* .---------.------.
	* | COMPILE | EXIT |
	* .---------.------.
	* Definition of ":", an immediate word:
	* .---.
	* | : |
	* .---.
	* Definition of ";", a defined immediate word:
	* .-----.----.-------.----.-----------.--------.-------.
	* | RUN | $' | $exit | $, | literal 0 | $state | $exit |
	* .-----.----.-------.----.-----------.--------.-------.
	* Definition of "square", a defined compiling word:
	* .---------.-----.------.----.-------.
	* | COMPILE | RUN | $dup | $* | $exit |
	* .---------.-----.------.----.-------.
	* Definition of "sum-of-products", a defined compiling word:
	* .---------.-----.---------.-------.---------.----.-------.
	* | COMPILE | RUN | $square | $swap | $square | $+ | $exit |
	* .---------.-----.---------.-------.---------.----.-------.
	*
	* All of these words are defined in the dictionary, which is a
	* separate data structure from the variable stack. In the above
	* definitions we use "$square" or "$*" to mean a pointer to the words
	* run time behavior, this is never the "COMPILE" field. "literal 0"
	* means that at run time the number 0 is pushed to the variable stack,
	* also the definition of "state" is not shown, as that would
	* complicate things.
	*
	* Imagine we have just typed in "sum-of-products" with "3 4" on the
	* variable stack. Our "pc" register is now pointing the "RUN" field of
	* sum of products, the virtual machine will next execute the "RUN"
	* instruction, saving the instruction pointer to the return stack for
	* when we finally exit "sum-of-products" back to the interpreter.
	* "square" will now be called, it's "RUN" field encountered, then "dup".
	* "dup" does not have a "RUN" field, it is a built in primitive, so the
	* instruction pointer will not be touched nor the return stack, but the
	* "DUP" instruction will now be executed. After this has run the
	* instruction pointer will now be moved to executed "*", another
	* primitive, then "exit" - which pops a value off the return stack and
	* sets the instruction pointer to that value. The value points to the
	* "$swap" field in "sum-of-products", which will in turn be executed
	* until the final "$exit" field is encountered. This exits back into
	* our special read-and-loop word defined in the initialization code.
	*
	* The "READ" routine must make sure the correct field is executed when
	* a word is read in which depends on the state of the interpreter (held
	* in STATE register). */
	for(;(pc = m[ck(I++)]);) { 
	INNER:  
		w = instruction(m[ck(pc++)]);
		TRACE(o, w, S, f);
		switch (w) { 

		/*When explaining words with example Forth code the
		* instructions enumeration will not be used (such as ADD or
		* SUB), but its name will be used instead (such as '+' or '-) */

		case PUSH:    *++S = f;     f = m[ck(I++)];          break;
		case COMPILE: m[ck(m[DIC]++)] = pc;                  break; 
		case RUN:     m[ck(++m[RSTK])] = I; I = pc;          break;
		case DEFINE:
			/*DEFINE backs the Forth word ':', which is an
			* immediate word, it reads in a new word name, creates
			* a header for that word and enters into compile mode,
			* where all words (baring immediate words) are
			* compiled into the dictionary instead of being
			* executed.
			*
			* The created header looks like this:
			*  .------.-----.------.-----.----
			*  | NAME | PWD | MISC | RUN |    ...
			*  .------.-----.------.-----.----
			*                               ^
			*                               |
			*                             Dictionary Pointer */
			m[STATE] = 1; /* compile mode */
			if(forth_get_word(o, o->s) < 0)
				goto end;
			compile(o, COMPILE, (char*)o->s);
			m[ck(m[DIC]++)] = RUN;
			break;
		case IMMEDIATE:
			/*IMMEDIATE makes the current word definition execute
			* regardless of whether we are in compile or command
			* mode. Unlike most Forths this needs to go right
			* after the word to be defined name instead of after
			* the word definition itself. I prefer this behavior,
			* however the reason for this is due to
			* implementation reasons and not because of this
			* preference.
			*
			* So our interpreter defines immediate words:
			*
			* : name immediate ... ;
			*
			* versus, as is expected:
			*
			* : name ... ; immediate
			*
			* The way this word works is when DEFINE (or ":")
			* runs it creates a word header that looks like this:
			*
			* .------.-----.------.-----.----
			* | NAME | PWD | MISC | RUN |    ...
			* .------.-----.------.-----.----
			*                              ^
			*                              |
			*                            Dictionary Pointer
			*
			*
			* Where the MISC field contains COMPILE, we want it
			* to look like this:
			*
			* .------.-----.------.----
			* | NAME | PWD | MISC |    ...
			* .------.-----.------.----
			*                        ^
			*                        |
			*                        Dictionary Pointer
			*
			* With the MISC field containing RUN. */
			m[DIC] -= 2; /* move to first code field */
			m[m[DIC]] &= ~INSTRUCTION_MASK; /* zero instruction */
			m[m[DIC]] |= RUN; /* set instruction to RUN */
			m[DIC]++; /* compilation start here */ break;
		case READ:
			/*The READ instruction, an instruction that
			* usually does not belong in a virtual machine,
			* forms the basis of Forths interactive nature.
			*
			* It attempts to do the follow:
			*
			* Lookup a space delimited string in the Forth
			* dictionary, if it is found and we are in
			* command mode we execute it, if we are in
			* compile mode and the word is a compiling word
			* we compile a pointer to it in the dictionary,
			* if not we execute it.
			*
			* If it is not a word in the dictionary we
			* attempt to treat it as a number, if it is a
			* number (using the BASE register to determine
			* the base of it) then if we are in command mode
			* we push the number to the variable stack, else
			* if we are in compile mode we compile the
			* literal into the dictionary.
			*
			* If it is neither a word nor a number,
			* regardless of mode, we emit a diagnostic.
			*
			* This is the most complex word in the Forth
			* virtual machine, there is a good case for it
			* being moved outside of it, and perhaps this
			* will happen. You will notice that the above
			* description did not include any looping, as
			* such there is a driver for the interpreter
			* which must be made and initialized in
			* "forth_init", a simple word that calls READ in
			* a loop (actually tail recursively). */
			if(forth_get_word(o, o->s) < 0)
				goto end;
			if ((w = forth_find(o, (char*)o->s)) > 1) {
				pc = w;
				if (!m[STATE] && instruction(m[ck(pc)]) == COMPILE)
					pc++; /* in command mode, execute word */
				goto INNER;
			} else if(!numberify(o->m[BASE], &w, (char*)o->s)) {
				fprintf(stderr, "( error \" %s is not a word\" )\n", o->s);
				longjmp(on_error, RECOVERABLE);
				break;
			}
			if (m[STATE]) { /* must be a number then */
				m[m[DIC]++] = 2; /*fake word push at m[2] */
				m[ck(m[DIC]++)] = w;
			} else { /* push word */
				*++S = f;
				f = w;
			}
			break;
		/*Most of the following Forth instructions are simple Forth
		* words, each one with an uncomplicated Forth word which is
		* implemented by the corresponding instruction (such as LOAD
		* and "@", STORE and "!", EXIT and "exit", and ADD and "+").
		*
		* However, the reason for these words existing, and under what
		* circumstances some of the can be used is a different matter,
		* the COMMA and TAIL word will require some explaining, but
		* ADD, SUB and DIV will not. */
		case LOAD:    cd(1); f = m[ck(f)];                   break;
		case STORE:   cd(2); m[ck(f)] = *S--; f = *S--;      break;
		case CLOAD:   cd(1); f = *(((uint8_t*)m) + ckchar(f)); break;
		case CSTORE:  cd(2); ((uint8_t*)m)[ckchar(f)] = *S--; f = *S--; break;
		case SUB:     cd(2); f = *S-- - f;                   break;
		case ADD:     cd(2); f = *S-- + f;                   break;
		case AND:     cd(2); f = *S-- & f;                   break;
		case OR:      cd(2); f = *S-- | f;                   break;
		case XOR:     cd(2); f = *S-- ^ f;                   break;
		case INV:     cd(1); f = ~f;                         break;
		case SHL:     cd(2); f = *S-- << f;                  break;
		case SHR:     cd(2); f = *S-- >> f;                  break;
		case MUL:     cd(2); f = *S-- * f;                   break;
		case DIV:
			cd(2);
			if(f) {
				f = *S-- / f;
			} else {
				fputs("( error \" divide by zero\" )\n", stderr);
				longjmp(on_error, RECOVERABLE);
			} 
			break;
		case ULESS:   cd(2); f = *S-- < f;                       break;
		case UMORE:   cd(2); f = *S-- > f;                       break;
		case SLESS:   cd(2); f = ((forth_signed_cell_t)(*S--)) < (forth_signed_cell_t)f; break;
		case SMORE:   cd(2); f = ((forth_signed_cell_t)(*S--)) > (forth_signed_cell_t)f; break;
		case EXIT:    I = m[ck(m[RSTK]--)];                      break;
		/* @note key and emit could be replaced by the file 
		 * primitives defined later */
		case EMIT:    cd(1); f = fputc(f, (FILE*)(o->m[FOUT]));  break;
		case KEY:     *++S = f; f = forth_get_char(o);           break;
		case FROMR:   *++S = f; f = m[ck(m[RSTK]--)];            break;
		case TOR:     cd(1); m[ck(++m[RSTK])] = f; f = *S--;     break;
		case BRANCH:  I += m[ck(I)];                             break;
		case QBRANCH: cd(1); I += f == 0 ? m[I] : 1; f = *S--;   break;
		case PNUM:    cd(1); 
			      f = print_cell(o, (FILE*)(o->m[FOUT]), f); break;
		case QUOTE:   *++S = f;     f = m[ck(I++)];              break;
		case COMMA:   cd(1); m[ck(m[DIC]++)] = f; f = *S--;      break;
		case EQUAL:   cd(2); f = *S-- == f;                      break;
		case SWAP:    cd(2); w = f;  f = *S--;   *++S = w;       break;
		case DUP:     cd(1); *++S = f;                           break;
		case DROP:    cd(1); f = *S--;                           break;
		case OVER:    cd(2); w = *S; *++S = f; f = w;            break;
		/*TAIL is a crude method of doing tail recursion, it should
		* not be used generally but is quite useful at startup, there
		* are a number of limitations when using it in word
		* definitions.
		*
		* The following tail recursive definition of the greatest
		* common divisor, called "(gcd)" will not work correctly when
		* interacting with other words:
		*
		* : (gcd) ?dup if dup rot rot mod tail (gcd) then ;
		*
		* If we define a word:
		*
		* : uses-gcd 50 20 (gcd) . ;
		*
		* We might expect it to print out "10", however it will not, it
		* will calculate the GCD, but not print it out with ".", as GCD
		* will have popped off where it should have returned.
		*
		* Instead we must wrap the definition up in another definition:
		*
		* : gcd (gcd) ;
		*
		* And the definition "gcd" can be used. There is a definition
		* of "tail" within "forth.fth" that does not have this
		* limitation, in fact the built in definition is hidden in
		* favor of the new one.*/
		case TAIL:
			m[RSTK]--;
			break;
		/* These two primitives implement block IO, they could be
		 * implemented in terms of the file primitives later, and so
		 * they may be removed. */
		case BSAVE:
			cd(2);
			f = blockio(o, *S--, f, 'w');
			break;
		case BLOAD:
			cd(2);
			f = blockio(o, *S--, f, 'r');
			break;
		/* FIND is a natural factor of READ, we add it to the Forth
		 * interpreter as it already exits, it looks up a Forth word in
		 * the dictionary and returns a pointer to that word if it
		 * found. */
		case FIND:
			*++S = f;
			if(forth_get_word(o, o->s) < 0)
				goto end;
			f = forth_find(o, (char*)o->s);
			f = f < DICTIONARY_START ? 0 : f;
			break;
		/* PRINT is a word that could be removed from the interpreter,
		 * as it could be implemented in terms of looping and emit, it
		 * prints out an ASCII delimited string to the output stream.
		 *
		 * There is a bit of an impedance mismatch between how Forth
		 * treats strings and how most programming languages treat
		 * them. Most higher level languages are built upon the C
		 * runtime, so at some level support NUL terminated strings,
		 * however Forth uses strings that include a pointer to the
		 * string and the strings length instead. As more C functions
		 * are added the difference in string treatment will become
		 * more apparent. Due to this difference it is always best to
		 * NUL terminate strings in Forth code even if they are stored
		 * with their length */
		case PRINT:
			cd(1);
			fputs(((char*)m)+f, (FILE*)(o->m[FOUT]));
			f = *S--;
			break;
		/* DEPTH is added because the stack is not directly accessible
		 * by the virtual machine (mostly for code readability
		 * reasons), normally it would have no way of knowing where the
		 * variable stack pointer is, which is needed to implement
		 * Forth words such as ".s" - which prints out all the items on
		 * the stack. */
		case DEPTH:
			w = S - o->vstart;
			*++S = f;
			f = w;
			break;
		/* CLOCK allows for a very primitive and wasteful (depending on
		 * how the C library implements "clock") timing mechanism, it
		 * has the advantage of being largely portable */
		case CLOCK:
			*++S = f;
			f = ((1000 * clock()) - clk) / CLOCKS_PER_SEC;
			break;
		/* EVALUATOR is another complex word needs to be implemented in
		 * the virtual machine. It mostly just saves and restores state
		 * which we do not usually need to do when the interpreter is
		 * not running (the usual case for forth_eval when called from
		 * C) */
		case EVALUATE:
		{ 
			/* save current input */
			forth_cell_t sin    = o->m[SIN],  sidx = o->m[SIDX],
				slen   = o->m[SLEN], fin  = o->m[FIN],
				source = o->m[SOURCE_ID], r = m[RSTK];
			cd(2);
			char *s = forth_get_string(o, &on_error, &S, f);
			f = *S--;
			/* save the stack variables */
			o->S = S;
			o->m[TOP] = f;
			/* push a fake call to forth_eval */
			m[RSTK]++;
			w = forth_eval(o, s);
			/* restore stack variables */
			m[RSTK] = r;
			S = o->S;
			*++S = o->m[TOP];
			f = w;
			/* restore input stream */
			o->m[SIN]  = sin;
			o->m[SIDX] = sidx;
			o->m[SLEN] = slen;
			o->m[FIN]  = fin;
			o->m[SOURCE_ID] = source;
			if(o->m[INVALID])
				return -1;
		}
		break;
		/* Whilst loathe to put these in here as virtual machine
		 * instructions (instead a better mechanism should be found),
		 * this is the simplest way of adding file access words to our
		 * Forth interpreter */
		case PSTK:    print_stack(o, (FILE*)(o->m[STDOUT]), S, f);   break;
		case RESTART: cd(1); longjmp(on_error, f);                   break;
		case SYSTEM:  cd(2); f = system(forth_get_string(o, &on_error, &S, f)); break;
		case FCLOSE:  cd(1); f = fclose((FILE*)f);                   break;
		case FDELETE: cd(2); f = remove(forth_get_string(o, &on_error, &S, f)); break;
		case FPOS:    cd(1); f = ftell((FILE*)f);                    break;
		case FSEEK:   cd(2); f = fseek((FILE*)f, *S--, SEEK_SET);    break;
		case FFLUSH:  cd(1); f = fflush((FILE*)f);                   break;
		case FRENAME:  
			cd(3); 
			{
				const char *f1 = forth_get_fam(&on_error, f);
				f = *S--;
				char *f2 = forth_get_string(o, &on_error, &S, f);
				f = rename(f2, f1);
			}
			break;
		case FOPEN: 
			cd(3);
			{
				const char *fam = forth_get_fam(&on_error, f);
				f = *S--;
				char *file = forth_get_string(o, &on_error, &S, f);
				f = (forth_cell_t)fopen(file, fam);
			}
			break;
		case FREAD:
			cd(3);
			{
				FILE *file = (FILE*)f;
				forth_cell_t count = *S--;
				forth_cell_t offset = *S--;
				*++S = fread(((char*)m)+offset, 1, count, file);
				f = ferror(file);
			}
			break;
		case FWRITE:
			cd(3);
			{
				FILE *file = (FILE*)f;
				forth_cell_t count = *S--;
				forth_cell_t offset = *S--;
				*++S = fwrite(((char*)m)+offset, 1, count, file);
				f = ferror(file);
			}
			break;
		break;
		/*This should never happen, and if it does it is an indication
		 *that virtual machine memory has been corrupted somehow */
		default:
			fprintf(stderr, "( fatal 'illegal-op %" PRIdCell " )\n", w);
			longjmp(on_error, FATAL);
		}
	}
	/*We must save the stack pointer and the top of stack when we exit the
	 * interpreter so the C functions like "forth_pop" work correctly. If the
	 * forth_t object has been invalidated (because something when very
	 * wrong), we do not have to jump to "end" as functions like
	 * "forth_pop" should not be called on the invalidated object any
	 * longer. */
end:	o->S = S;
	o->m[TOP] = f;
	return 0;
}

/* ============================ Section 6 ================================== */
/*    An example main function called main_forth and support functions       */

/* This section is not needed to understand how Forth works, or how the C API
 * into the Forth interpreter works. It provides a function which uses all
 * the functions available to the API programmer in order to create an example
 * program that implements a Forth interpreter with a Command Line Interface.
 *
 * This program can be used as a filter in a Unix pipe chain, or as a
 * standalone interpreter for Forth. It tries to follow the Unix philosophy and
 * way of doing things (see
 * http://www.catb.org/esr/writings/taoup/html/ch01s06.html and
 * https://en.wikipedia.org/wiki/Unix_philosophy). Whether this is achieved
 * is a matter of opinion. There are a few things this interpreter does
 * differently to most Forth interpreters that support this philosophy however,
 * it is silent by default and does not clutter up the output window with "ok",
 * or by printing a banner at start up (which would contain no useful
 * information whatsoever). It is simple, and only does one thing (but does it
 * do it well?). */
static void fclose_input(FILE **in)
{
	if(*in && (*in != stdin))
		fclose(*in);
	*in = stdin;
}

void forth_set_args(forth_t *o, int argc, char **argv)
{ /* currently this is of little use to the interpreter */
	assert(o);
	o->m[ARGC] = argc;
	o->m[ARGV] = (forth_cell_t)argv;
}

/* main_forth implements a Forth interpreter which is a wrapper around the C
 * API, there is an assumption that main_forth will be the only thing running
 * in a process (it does not seem sensible to run multiple instances of it at
 * the same time - it is just for demonstration purposes), as such the only
 * error handling should do is to die after printing an error message if an
 * error occurs, the "fopen_or_die" is an example of this philosophy, one which
 * does not apply to functions like "forth_run" (which makes attempts to
 * recover from a sensible error). */
static FILE *fopen_or_die(const char *name, char *mode)
{
	errno = 0;
	FILE *file = fopen(name, mode);
	if(!file) {
		fprintf(stderr, "( fatal 'file-open \"%s: %s\" )\n", name, errno ? strerror(errno): "unknown");
		exit(EXIT_FAILURE);
	}
	return file;
}

static void usage(const char *name)
{
	fprintf(stderr, "usage: %s [-s file] [-e string] [-l file] [-t] [-h] [-v] [-m size] [-] files\n", name);
}

/* We try to keep the interface to the example program as simple as possible, so
 * there are few options and they are largely uncomplicated. What they do
 * should come as no surprise to an experienced Unix programmer, it is
 * important to pick option names that they would expect (for example "-l" for
 * loading, "-e" for evaluation, and not using "-h" for help would be a hanging
 * offense). */
static void help(void)
{
	static const char help_text[] = "\
Forth: A small forth interpreter build around libforth\n\n\
\t-h        print out this help and exit unsuccessfully\n\
\t-e string evaluate a string\n\
\t-s file   save state of forth interpreter to file\n\
\t-d        save state to 'forth.core'\n\
\t-l file   load previously saved state from file\n\
\t-m size   specify forth memory size in kilobytes (cannot be used with '-l')\n\
\t-t        process stdin after processing forth files\n\
\t-v        turn verbose mode on\n\
\t-         stop processing options\n\n\
Options must come before files to execute\n\n";
	fputs(help_text, stderr);
}

/* "main_forth" is the second largest function is this file, but is not as
 * complex as forth_run (currently the largest and most complex function), it
 * brings together all the API functions offered by this library and provides
 * a quick way for programmers to implement a working Forth interpreter for
 * testing purposes.
 *
 * This make implementing a Forth interpreter as simple as:
 *
 * ==== main.c =============================
 *
 *   #include "libforth.h"
 *
 *   int main(int argc, char **argv)
 *   {
 *           return main_forth(argc, argv);
 *   }
 *
 * ==== main.c =============================
 *
 * To keep things simple options are parsed first then arguments like files,
 * although some options take arguments immediately after them. */
int main_forth(int argc, char **argv)
{
	FILE *in = NULL, *dump = NULL;
	int save = 0, readterm = 0, mset = 0, eval = 0, rval = 0, i = 1, c = 0, verbose = 0;
	static const size_t kbpc = 1024/sizeof(forth_cell_t); /*kilobytes per cell */
	static const char *dump_name = "forth.core";
	char *optarg = NULL;
	forth_cell_t core_size = DEFAULT_CORE_SIZE;
	forth_t *o = NULL;
	/* This loop processes any options that may have been passed to the
	 * program, it looks for arguments beginning with '-' and attempts to
	 * process that option, if the argument does not start with '-' the
	 * option processing stops. It is a simple mechanism for processing
	 * program arguments and there are better ways of doing it (such as
	 * "getopt" and "getopts"), but by using them we sacrifice portability. */

	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		switch(argv[i][1]) {
		case '\0': goto done; /* stop argument processing */
		case 'h':  usage(argv[0]); help(); return -1;
		case 't':  readterm = 1; 
			   if(verbose)
				   fprintf(stderr, "note: read from stdin after processing arguments\n");
			   break;
		case 'e':
			if(i >= (argc - 1))
				goto fail;
			if(!(o = o ? o : forth_init(core_size, stdin, stdout))) {
				fprintf(stderr, "fatal: initialization failed\n");
				return -1;
			}
			optarg = argv[++i];
			if(verbose)
				fprintf(stderr, "note: evaluating '%s'\n", optarg);
			if(forth_eval(o, optarg) < 0)
				goto end;
			eval = 1;
			break;
		case 's':
			if(i >= (argc - 1))
				goto fail;
			dump_name = argv[++i];
		case 'd':  /*use default name */
			if(verbose)
				fprintf(stderr, "note: saving core file to '%s' (on exit)\n", dump_name);
			save = 1;
			break;
		case 'm':
			if(o || (i >= argc - 1) || !numberify(10, &core_size, argv[++i]))
				goto fail;
			if((core_size *= kbpc) < MINIMUM_CORE_SIZE) {
				fprintf(stderr, "fatal: -m too small (minimum %zu)\n", MINIMUM_CORE_SIZE / kbpc);
				return -1;
			}
			if(verbose)
				fprintf(stderr, "note: memory size set to %zu\n", core_size);
			mset = 1;
			break;
		case 'l':
			if(o || mset || (i >= argc - 1))
				goto fail;
			optarg = argv[++i];
			if(verbose)
				fprintf(stderr, "note: loading core file '%s'", optarg);
			if(!(o = forth_load_core(dump = fopen_or_die(optarg, "rb")))) {
				fprintf(stderr, "fatal: %s: core load failed\n", optarg);
				return -1;
			}
			fclose(dump);
			break;
		case 'v':
			verbose++;
			break;
		default:
		fail:
			fprintf(stderr, "fatal: invalid argument '%s'\n", argv[i]);
			usage(argv[0]);
			return -1;
		}
done:
	readterm = (!eval && i == argc) || readterm; /* if no files are given, read stdin */
	if(!o && !(o = forth_init(core_size, stdin, stdout))) {
		fprintf(stderr, "fatal: forth initialization failed\n");
		return -1;
	}
	forth_set_args(o, argc, argv);
	for(; i < argc; i++) { /* process all files on command line */
		if(verbose)
			fprintf(stderr, "note: reading from file '%s'\n", argv[i]);
		forth_set_file_input(o, in = fopen_or_die(argv[i], "rb"));
		if((c = fgetc(in)) == '#') /*shebang line '#!', core files could also be detected */
			while(((c = forth_get_char(o)) > 0) && (c != '\n'));
		else if(c == EOF)
			goto close;
		else
			ungetc(c, in);
		if((rval = forth_run(o)) < 0)
			goto end;
close:	
		fclose_input(&in);
	}
	if(readterm) { /* if '-t' or no files given, read from stdin */
		if(verbose)
			fprintf(stderr, "note: reading from stdin\n");
		forth_set_file_input(o, stdin);
		rval = forth_run(o);
	}
end:	
	fclose_input(&in);
	/* If the save option has been given we only want to save valid core
	 * files, we might want to make an option to force saving of core files
	 * for debugging purposes, but in general we do not want to over write
	 * valid previously saved state with invalid data. */
	if(save) { /* save core file */
		if(rval || o->m[INVALID]) {
			fprintf(stderr, "fatal: refusing to save invalid core\n");
			return -1;
		}
		if(verbose)
			fprintf(stderr, "note: saving for file to '%s'\n", dump_name);
		if(forth_save_core(o, dump = fopen_or_die(dump_name, "wb"))) {
			fprintf(stderr, "fatal: core file save to '%s' failed\n", dump_name);
			rval = -1;
		}
		fclose(dump);
	}
	/* Whilst the following "forth_free" is not strictly necessary, there
	 * is often a debate that comes up making short lived programs or
	 * programs whose memory use stays either constant or only goes up,
	 * when these programs exit it is not necessary to clean up the
	 * environment and in some case (although not this one) it can
	 * significantly slow down the exit of the program for no reason.
	 * However not freeing the memory after use does not play nice with
	 * programs that detect memory leaks, like Valgrind. Either way, we
	 * free the memory used here, but only if no other errors have occurred
	 * before hand. */
	forth_free(o);
	return rval;
}

