/**
# libforth.c.md
@file       libforth.c
@author     Richard James Howe.
@copyright  Copyright 2015,2016 Richard James Howe.
@license    MIT
@email      howe.r.j.89@gmail.com

@brief      A FORTH library, written in a literate style.

## License

The MIT License (MIT)

Copyright (c) 2016 Richard James Howe

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

This file implements the core Forth interpreter, it is written in portable
C99. The file contains a virtual machine that can interpret threaded Forth
code and a simple compiler for the virtual machine, which is one of its
instructions. The interpreter can be embedded in another application and
there should be no problem instantiating multiple instances of the
interpreter.

**/

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CORE_SIZE (2048)

typedef uintptr_t forth_cell_t; /**< FORTH cell large enough for a pointer*/

#define PRIdCell PRIdPTR /**< Decimal format specifier for a Forth cell */
#define PRIxCell PRIxPTR /**< Hex format specifier for a Forth word */

static const char *emsg(void);
static int logger(const char *prefix, const char *func, 
		unsigned line, const char *fmt, ...);
static int forth_run(void); 

#define fatal(FMT,...)   logger("fatal",  __func__, __LINE__, FMT, __VA_ARGS__)
#define error(FMT,...)   logger("error",  __func__, __LINE__, FMT, __VA_ARGS__)
#define warning(FMT,...) logger("warning",__func__, __LINE__, FMT, __VA_ARGS__)
#define note(FMT,...)    logger("note",   __func__, __LINE__, FMT, __VA_ARGS__)
#define debug(FMT,...)   logger("debug",  __func__, __LINE__, FMT, __VA_ARGS__)

#ifndef NDEBUG
#define ck(C) check_bounds((C), __LINE__, CORE_SIZE)
#define ckchar(C) check_bounds((C), __LINE__, \
			CORE_SIZE * sizeof(forth_cell_t))
#define cd(DEPTH) check_depth(S, (DEPTH), __LINE__)
#define dic(DPTR) check_dictionary((DPTR))
#define TRACE(INSTRUCTION,STK,TOP) trace(INSTRUCTION,STK,TOP)
#else
#define ck(C) (C)
#define ckchar(C) (C)
#define cd(DEPTH) ((void)DEPTH)
#define dic(DPTR) check_dictionary((DPTR))
#define TRACE(INSTRUCTION, STK, TOP)
#endif

/**
@brief Default VM size 
**/
#define DEFAULT_CORE_SIZE   (32 * 1024) 
#define BLOCK_SIZE          (1024u)
#define STRING_OFFSET       (32u)
#define MAXIMUM_WORD_LENGTH (32u)
#define MINIMUM_STACK_SIZE  (64u)
#define DICTIONARY_START (STRING_OFFSET+MAXIMUM_WORD_LENGTH) /**< start of dic*/
#define WORD_LENGTH_OFFSET  (8)  
#define WORD_LENGTH(MISC) (((MISC) >> WORD_LENGTH_OFFSET) & 0xff)
#define WORD_HIDDEN(MISC) ((MISC) & 0x80)
#define INSTRUCTION_MASK    (0x7f)
#define instruction(k)      ((k) & INSTRUCTION_MASK)
#define VERIFY(X)           do { if(!(X)) { abort(); } } while(0)
#define IS_BIG_ENDIAN (!(union { uint16_t u16; uint8_t c; }){ .u16 = 1 }.c)
#define CORE_VERSION        (0x02u)

static const char *initial_forth_program = 
": here h @ ; \n"
": [ immediate 0 state ! ; \n"
": ] 1 state ! ; \n"
": >mark here 0 , ; \n"
": :noname immediate -1 , here 2 , ] ; \n"
": if immediate ' ?branch , >mark ; \n"
": else immediate ' branch , >mark swap dup here swap - swap ! ; \n"
": then immediate dup here swap - swap ! ; \n"
": begin immediate here ; \n"
": until immediate ' ?branch , here - , ; \n"
": ')' 41 ; \n"
": ( immediate begin key ')' = until ; \n"
": rot >r swap r> swap ; \n"
": -rot rot rot ; \n"
": tuck swap over ; \n"
": nip swap drop ; \n"
": allot here + h ! ; \n"
": 2drop drop drop ; \n"
": bl 32 ; \n"
": emit _emit drop ; \n"
": space bl emit ; \n"
": . pnum drop space ; \n";

static const char conv[] = "0123456789abcdefghijklmnopqrstuvwxzy";

enum errors
{
	INITIALIZED, /**< setjmp returns zero if returning directly */
	OK,          /**< no error, do nothing */
	FATAL,       /**< fatal error, this invalidates the Forth image */
	RECOVERABLE, /**< recoverable error, this will reset the interpreter */
};

struct forth { /**< FORTH environment */
	uint8_t *s;          /**< convenience pointer for string input buffer */
	char hex_fmt[16];    /**< calculated hex format */
	char word_fmt[16];   /**< calculated word format */
	forth_cell_t *S;     /**< stack pointer */
	forth_cell_t *vstart;/**< index into m[] where the variable stack starts*/
	forth_cell_t *vend;  /**< index into m[] where the variable stack ends*/
	forth_cell_t m[CORE_SIZE];    /**< ~~ Forth Virtual Machine memory */
};

static struct forth o;
static jmp_buf on_error;

enum actions_on_error
{
	ERROR_RECOVER,    /**< recover when an error happens, like a call to ABORT */
	ERROR_HALT,       /**< halt on error */
	ERROR_INVALIDATE, /**< halt on error and invalid the Forth interpreter */
};

enum trace_level
{
	DEBUG_OFF,         /**< tracing is off */
	DEBUG_INSTRUCTION, /**< instructions and stack are traced */
	DEBUG_CHECKS       /**< bounds checks are printed out */
};

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

enum input_stream {
	FILE_IN,       /**< file input; this could be interactive input */
	STRING_IN = -1 /**< string input */
};

static const char *register_names[] = { "h", "r", "`state", "base", "pwd",
"`source-id", "`sin", "`sidx", "`slen", "`start-address", "`fin", "`fout", 
"`stdin", "`stdout", "`stderr", "`argc", "`argv", "`debug", "`invalid", 
"`top", "`instruction", "`stack-size", "`error-handler", NULL };

#define XMACRO_INSTRUCTIONS\
 X(PUSH,      "push",       " -- x : push a literal")\
 X(COMPILE,   "compile",    " -- : compile a pointer to a Forth word")\
 X(RUN,       "run",        " -- : run a Forth word")\
 X(DEFINE,    "define",     " -- : make new Forth word, set compile mode")\
 X(IMMEDIATE, "immediate",  " -- : make a Forth word immediate")\
 X(READ,      "read",       " -- : read in a Forth word and execute it")\
 X(LOAD,      "@",          "addr -- x : load a value")\
 X(STORE,     "!",          "x addr -- : store a value")\
 X(CLOAD,     "c@",         "c-addr -- x : load character value")\
 X(CSTORE,    "c!",         "x c-addr -- : store character value")\
 X(SUB,       "-",          "x1 x2 -- x3 : subtract x2 from x1 yielding x3")\
 X(ADD,       "+",          "x x -- x : add two values")\
 X(AND,       "and",        "x x -- x : bitwise and of two values")\
 X(OR,        "or",         "x x -- x : bitwise or of two values")\
 X(XOR,       "xor",        "x x -- x : bitwise exclusive or of two values")\
 X(INV,       "invert",     "x -- x : invert bits of value")\
 X(SHL,       "lshift",     "x1 x2 -- x3 : left shift x1 by x2")\
 X(SHR,       "rshift",     "x1 x2 -- x3 : right shift x1 by x2")\
 X(MUL,       "*",          "x x -- x : multiply to values")\
 X(DIV,       "/",          "x1 x2 -- x3 : divide x1 by x2 yielding x3")\
 X(ULESS,     "u<",         "x x -- bool : unsigned less than")\
 X(UMORE,     "u>",         "x x -- bool : unsigned greater than")\
 X(EXIT,      "exit",       " -- : return from a word defition")\
 X(KEY,       "key",        " -- char : get one character of input")\
 X(EMIT,      "_emit",      "char -- bool : emit one character to output")\
 X(FROMR,     "r>",         " -- x, R: x -- : move from return stack")\
 X(TOR,       ">r",         "x --, R: -- x : move to return stack")\
 X(BRANCH,    "branch",     " -- : unconditional branch")\
 X(QBRANCH,   "?branch",    "x -- : branch if x is zero")\
 X(PNUM,      "pnum",       "x -- : print a number")\
 X(QUOTE,     "'",          " -- addr : push address of word")\
 X(COMMA,     ",",          "x -- : write a value into the dictionary")\
 X(EQUAL,     "=",          "x x -- bool : compare two values for equality")\
 X(SWAP,      "swap",       "x1 x2 -- x2 x1 : swap two values")\
 X(DUP,       "dup",        "x -- x x : duplicate a value")\
 X(DROP,      "drop",       "x -- : drop a value")\
 X(OVER,      "over",       "x1 x2 -- x1 x2 x1 : copy over a value")\
 X(TAIL,      "tail",       " -- : tail recursion")\
 X(BSAVE,     "bsave",      "c-addr x -- : save a block")\
 X(BLOAD,     "bload",      "c-addr x -- : load a block")\
 X(FIND,      "find",       "c\" xxx\" -- addr | 0 : find a Forth word")\
 X(DEPTH,     "depth",      " -- x : get current stack depth")\
 X(CLOCK,     "clock",      " -- x : push a time value")\
 X(EVALUATE,  "evaluate",   "c-addr u -- x : evaluate a string")\
 X(PSTK,      ".s",         " -- : print out values on the stack")\
 X(RESTART,   "restart",         " error -- : restart system, cause error")\
 X(HELP,      "help",       " -- : print a help message")\
 X(LAST_INSTRUCTION, NULL, "")\

enum instructions { /**< instruction enumerations */
#define X(ENUM, STRING, HELP) ENUM,
	XMACRO_INSTRUCTIONS
#undef X
};

static const char *instruction_names[] = { /**< instructions with names */
#define X(ENUM, STRING, HELP) STRING,
	XMACRO_INSTRUCTIONS
#undef X
};

static const char *instruction_help_strings[] = {
#define X(ENUM, STRING, HELP) HELP,
	XMACRO_INSTRUCTIONS
#undef X
};

static const char *emsg(void)
{
	static const char *unknown = "unknown reason";
	const char *r = errno ? strerror(errno) : unknown;
	if(!r) 
		r = unknown;
	return r;
}

static int logger(const char *prefix, const char *func, 
		unsigned line, const char *fmt, ...)
{
	int r;
	va_list ap;
	assert(prefix && func && fmt);
	fprintf(stderr, "[%s %u] %s: ", func, line, prefix);
	va_start(ap, fmt);
	r = vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	return r;
}

static int forth_get_char(void)
{
       switch(o.m[SOURCE_ID]) {
       case FILE_IN:   return fgetc((FILE*)(o.m[FIN]));
       case STRING_IN: return o.m[SIDX] >= o.m[SLEN] ? 
                               EOF : 
                               ((char*)(o.m[SIN]))[o.m[SIDX]++];
       default:        return EOF;
       }
}

static int forth_get_word(uint8_t *p)
{
	int n = 0;
	switch(o.m[SOURCE_ID]) {
	case FILE_IN:   return fscanf((FILE*)(o.m[FIN]), o.word_fmt, p, &n);
	case STRING_IN:
		if(sscanf((char *)&(((char*)(o.m[SIN]))[o.m[SIDX]]), o.word_fmt, p, &n) < 0)
			return EOF;
		o.m[SIDX] += n;
		return n;
	default:       return EOF;
	}
}

static void compile(forth_cell_t code, const char *str)
{ 
	assert(code < LAST_INSTRUCTION);
	forth_cell_t *m = o.m, header = m[DIC], l = 0;
	/*FORTH header structure */
	/*Copy the new FORTH word into the new header */
	strcpy((char *)(o.m + header), str); 
	/* align up to size of cell */
	l = strlen(str) + 1;
	l = (l + (sizeof(forth_cell_t) - 1)) & ~(sizeof(forth_cell_t) - 1); 
	l = l/sizeof(forth_cell_t);
	m[DIC] += l; /* Add string length in words to header (STRLEN) */

	m[m[DIC]++] = m[PWD]; /*0 + STRLEN: Pointer to previous words header */
	m[PWD] = m[DIC] - 1;  /*Update the PWD register to new word */
	/*size of words name and code field*/
	m[m[DIC]++] = (l << WORD_LENGTH_OFFSET) | code; 
}

static int blockio(forth_cell_t poffset, forth_cell_t id, char rw)
{ 
	char name[16] = {0}; /* XXXX + ".blk" + '\0' + a little spare change */
	FILE *file = NULL;
	size_t n;
	if(((forth_cell_t)poffset) > ((CORE_SIZE * sizeof(forth_cell_t)) - BLOCK_SIZE))
		return -1;
	sprintf(name, "%04x.blk", (int)id);
	errno = 0;
	if(!(file = fopen(name, rw == 'r' ? "rb" : "wb"))) { 
		error("file open %s, %s", name, emsg());
		return -1;
	}
	n = rw == 'w' ? fwrite(((char*)o.m) + poffset, 1, BLOCK_SIZE, file):
			fread (((char*)o.m) + poffset, 1, BLOCK_SIZE, file);
	fclose(file);
	return n == BLOCK_SIZE ? 0 : -1;
}

static int numberify(int base, forth_cell_t *n, const char *s)
{
	char *end = NULL;
	errno = 0;
	*n = strtol(s, &end, base);
	return !errno && *s != '\0' && *end == '\0';
}

static int istrcmp(const char *a, const char *b)
{
	for(; ((*a == *b) || (tolower(*a) == tolower(*b))) && *a && *b; a++, b++)
		;
	return tolower(*a) - tolower(*b);
}

static forth_cell_t forth_find(const char *s)
{
	forth_cell_t *m = o.m, w = m[PWD], len = WORD_LENGTH(m[w+1]);
	for (;w > DICTIONARY_START && (WORD_HIDDEN(m[w+1]) || istrcmp(s,(char*)(&o.m[w-len])));) {
		w = m[w];
		len = WORD_LENGTH(m[w+1]);
	}
	return w > DICTIONARY_START ? w+1 : 0;
}

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

static int print_cell(FILE *output, forth_cell_t f)
{
	unsigned base = o.m[BASE];
	if(base == 10 || base == 0)
		return fprintf(output, "%"PRIdCell, f);
	if(base == 16)
		return fprintf(output, o.hex_fmt, f);
	if(base == 1 || base > 36)
		return -1;
	return print_unsigned_number(f, base, output);
}

static forth_cell_t check_bounds(forth_cell_t f, unsigned line, forth_cell_t bound)
{
	if(o.m[DEBUG] >= DEBUG_CHECKS)
		debug("0x%"PRIxCell " %u", f, line);
	if(f >= bound) {
		fatal("bounds check failed (%" PRIdCell " >= %zu)", f, (size_t)bound);
		longjmp(on_error, FATAL);
	}
	return f;
}

static void check_depth(forth_cell_t *S, forth_cell_t expected, unsigned line)
{
	if(o.m[DEBUG] >= DEBUG_CHECKS)
		debug("0x%"PRIxCell " %u", (forth_cell_t)(S - o.vstart), line);
	if((uintptr_t)(S - o.vstart) < expected) {
		error("stack underflow %p", S);
		longjmp(on_error, RECOVERABLE);
	} else if(S > o.vend) {
		error("stack overflow %p", S - o.vend);
		longjmp(on_error, RECOVERABLE);
	}
}

static forth_cell_t check_dictionary(forth_cell_t dptr)
{
	if((o.m + dptr) >= (o.vstart)) {
		fatal("dictionary pointer is in stack area %"PRIdCell, dptr);
		o.m[INVALID] = 1;
		longjmp(on_error, FATAL);
	}
	return dptr;
}

static void check_is_asciiz(const char *s, forth_cell_t end)
{
	if(*(s + end) != '\0') {
		error("not an ASCIIZ string at %p", s);
		longjmp(on_error, RECOVERABLE);
	}
}

static char *forth_get_string(forth_cell_t **S, forth_cell_t f)
{
	forth_cell_t length = f;
	char *string = ((char*)o.m) + **S;
	(*S)--;
	check_is_asciiz(string, length);
	return string;
}

static void print_stack(FILE *out, forth_cell_t *S, forth_cell_t f)
{ 
	forth_cell_t depth = (forth_cell_t)(S - o.vstart);
	fprintf(out, "%"PRIdCell": ", depth);
	if(!depth)
		return;
	print_cell(out, f);
	fputc(' ', out);
	while(o.vstart + 1 < S) {
		print_cell(out, *(S--));
		fputc(' ', out);
	}
	fputc('\n', out);
}

static void trace(forth_cell_t instruction, forth_cell_t *S, forth_cell_t f)
{
	if(o.m[DEBUG] < DEBUG_INSTRUCTION)
		return;
	if(instruction > LAST_INSTRUCTION) {
		error("traced invalid instruction %"PRIdCell, instruction);
		return;
	}
	fprintf(stderr, "\t( %s\t ", instruction_names[instruction]);
	print_stack(stderr, S, f);
	fputs(" )\n", stderr);
}

static void help(void)
{
	fputs("Static Forth Help\n"
		"\tAuthor: Richard Howe\n"
		"\tLicense: MIT\n"
		"\tCopyright: Richard Howe, 2016\n", 
		stderr);
	fputs("Instruction List:\n", stderr);
	for(unsigned i = 0; i < LAST_INSTRUCTION; i++)
		fprintf(stderr, "%s\t\t%s\n",
				instruction_names[i],
				instruction_help_strings[i]);

}

static void forth_set_file_input(FILE *in)
{
	assert(in);
	o.m[SOURCE_ID] = FILE_IN;
	o.m[FIN]       = (forth_cell_t)in;
}

/*static void forth_set_file_output(FILE *out)
{
	assert(out);
	o.m[FOUT] = (forth_cell_t)out;
}*/

static void forth_set_string_input(const char *s)
{
	assert(s);
	o.m[SIDX] = 0;              /* m[SIDX] == current character in string */
	o.m[SLEN] = strlen(s) + 1;  /* m[SLEN] == string len */
	o.m[SOURCE_ID] = STRING_IN; /* read from string, not a file handle */
	o.m[SIN] = (forth_cell_t)s; /* sin  == pointer to string input */
}

static int forth_eval(const char *s)
{
	assert(s);
	forth_set_string_input(s);
	return forth_run();
}

static int forth_define_constant(const char *name, forth_cell_t c)
{
	char e[MAXIMUM_WORD_LENGTH+32] = {0};
	assert(strlen(name) < MAXIMUM_WORD_LENGTH);
	sprintf(e, ": %31s %" PRIdCell " ; \n", name, c);
	return forth_eval(e);
}

static void forth_make_default(FILE *in, FILE *out)
{
	assert(in && out);
	o.m[STACK_SIZE] = CORE_SIZE / MINIMUM_STACK_SIZE > MINIMUM_STACK_SIZE ?
				CORE_SIZE / MINIMUM_STACK_SIZE :
				MINIMUM_STACK_SIZE;

	o.s             = (uint8_t*)(o.m + STRING_OFFSET); /*skip registers*/
	o.m[FOUT]       = (forth_cell_t)out;
	o.m[START_ADDR] = (forth_cell_t)&(o.m);
	o.m[STDIN]      = (forth_cell_t)stdin;
	o.m[STDOUT]     = (forth_cell_t)stdout;
	o.m[STDERR]     = (forth_cell_t)stderr;
	o.m[RSTK] = CORE_SIZE - o.m[STACK_SIZE]; /* set up return stk ptr */
	o.m[ARGC] = o.m[ARGV] = 0;
	o.S       = o.m + CORE_SIZE - (2 * o.m[STACK_SIZE]); /* v. stk pointer */
	o.vstart  = o.m + CORE_SIZE - (2 * o.m[STACK_SIZE]);
	o.vend    = o.vstart + o.m[STACK_SIZE];
	sprintf(o.hex_fmt, "0x%%0%d"PRIxCell, (int)sizeof(forth_cell_t)*2);
	sprintf(o.word_fmt, "%%%ds%%n", MAXIMUM_WORD_LENGTH - 1);
	forth_set_file_input(in);  /* set up input after our eval */
}

static void forth_init(FILE *in, FILE *out)
{
	assert(in && out);
	forth_cell_t *m, i, w, t;
	assert(sizeof(forth_cell_t) >= sizeof(uintptr_t));

	forth_make_default(in, out);

	m = o.m;       /*a local variable only for convenience */

	o.m[PWD]   = 0;  /* special terminating pwd value */
	t = m[DIC] = DICTIONARY_START; /* initial dictionary offset */
	m[m[DIC]++] = TAIL; /* add a TAIL instruction that can be called */
	w = m[DIC];         /* save current offset, which will contain READ */
	m[m[DIC]++] = READ; /* populate the cell with READ */
	m[m[DIC]++] = RUN;  /* call the special word recursively */
	o.m[INSTRUCTION] = m[DIC]; /* stream points to the special word */
	m[m[DIC]++] = w;    /* call to READ word */
	m[m[DIC]++] = t;    /* call to TAIL */
	m[m[DIC]++] = o.m[INSTRUCTION] - 1; /* recurse*/

	compile(DEFINE,    ":");
	compile(IMMEDIATE, "immediate");

	for(i = READ, w = READ; instruction_names[i]; i++) {
		compile(COMPILE, instruction_names[i]);
		m[m[DIC]++] = w++; /*This adds the actual VM instruction */
	}

	VERIFY(forth_eval(": state 8 exit : ; immediate ' exit , 0 state ! ;") >= 0);

	for(i = 0; register_names[i]; i++)
		VERIFY(forth_define_constant(register_names[i], i+DIC) >= 0);

	VERIFY(forth_define_constant("size", sizeof(forth_cell_t)) >= 0);
	VERIFY(forth_define_constant("stack-start", CORE_SIZE - (2 * o.m[STACK_SIZE])) >= 0);
	VERIFY(forth_define_constant("max-core", CORE_SIZE ) >= 0);
	VERIFY(forth_define_constant("dictionary-start",  DICTIONARY_START) >= 0);
	VERIFY(forth_define_constant(">in",  STRING_OFFSET * sizeof(forth_cell_t)) >= 0);

	VERIFY(forth_eval(initial_forth_program) >= 0);

	forth_set_file_input(in);  /*set up input after our eval */
}

static int forth_run(void)
{
	int errorval = 0;
	if(o.m[INVALID]) {
		fatal("refusing to run an invalid forth, %"PRIdCell, o.m[INVALID]);
		return -1;
	}

	if ((errorval = setjmp(on_error)) || o.m[INVALID]) {
		/* If the interpreter gets into an invalid state we always
		 * exit, which */
		if(o.m[INVALID])
			return -1;
		switch(errorval) {
			default:
			case FATAL:
				return -(o.m[INVALID] = 1);
			/* recoverable errors depend on o.m[ERROR_HANDLER],
			 * a register which can be set within the running
			 * virtual machine. */
			case RECOVERABLE:
				switch(o.m[ERROR_HANDLER]) {
				case ERROR_INVALIDATE: o.m[INVALID] = 1;
				case ERROR_HALT:       return -(o.m[INVALID]);
				case ERROR_RECOVER:    o.m[RSTK] = CORE_SIZE - o.m[STACK_SIZE];
						       break;
				}
			case OK: break;
		}
	}
	
	forth_cell_t *m = o.m, pc, *S = o.S, I = o.m[INSTRUCTION], f = o.m[TOP], w, clk;

	clk = (1000 * clock()) / CLOCKS_PER_SEC;
	for(;(pc = m[ck(I++)]);) { 
	INNER:  
		w = instruction(m[ck(pc++)]);
		TRACE(w, S, f);
		switch (w) { 
		case PUSH:    *++S = f;     f = m[ck(I++)];          break;
		case COMPILE: m[dic(m[DIC]++)] = pc;                 break; 
		case RUN:     m[ck(++m[RSTK])] = I; I = pc;          break;
		case DEFINE:
			m[STATE] = 1; /* compile mode */
			if(forth_get_word(o.s) < 0)
				goto end;
			compile(COMPILE, (char*)o.s);
			m[dic(m[DIC]++)] = RUN;
			break;
		case IMMEDIATE:
			m[DIC] -= 2; /* move to first code field */
			m[m[DIC]] &= ~INSTRUCTION_MASK; /* zero instruction */
			m[m[DIC]] |= RUN; /* set instruction to RUN */
			dic(m[DIC]++); /* compilation start here */ 
			break;
		case READ:
			if(forth_get_word(o.s) < 0)
				goto end;
			if ((w = forth_find((char*)o.s)) > 1) {
				pc = w;
				if (!m[STATE] && instruction(m[ck(pc)]) == COMPILE)
					pc++; /* in command mode, execute word */
				goto INNER;
			} else if(!numberify(o.m[BASE], &w, (char*)o.s)) {
				error("'%s' is not a word", o.s);
				longjmp(on_error, RECOVERABLE);
				break;
			}
			if (m[STATE]) { /* must be a number then */
				m[dic(m[DIC]++)] = 2; /*fake word push at m[2] */
				m[dic(m[DIC]++)] = w;
			} else { /* push word */
				*++S = f;
				f = w;
			}
			break;
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
				error("divide %"PRIdCell" by zero ", *S--);
				longjmp(on_error, RECOVERABLE);
			} 
			break;
		case ULESS:   cd(2); f = *S-- < f;                       break;
		case UMORE:   cd(2); f = *S-- > f;                       break;
		case EXIT:    I = m[ck(m[RSTK]--)];                      break;
		case KEY:     *++S = f; f = forth_get_char();            break;
		case EMIT:    f = fputc(f, (FILE*)(o.m[FOUT]));          break;
		case FROMR:   *++S = f; f = m[ck(m[RSTK]--)];            break;
		case TOR:     cd(1); m[ck(++m[RSTK])] = f; f = *S--;     break;
		case BRANCH:  I += m[ck(I)];                             break;
		case QBRANCH: cd(1); I += f == 0 ? m[I] : 1; f = *S--;   break;
		case PNUM:    cd(1); 
			      f = print_cell((FILE*)(o.m[FOUT]), f); break;
		case QUOTE:   *++S = f;     f = m[ck(I++)];              break;
		case COMMA:   cd(1); m[dic(m[DIC]++)] = f; f = *S--;     break;
		case EQUAL:   cd(2); f = *S-- == f;                      break;
		case SWAP:    cd(2); w = f;  f = *S--;   *++S = w;       break;
		case DUP:     cd(1); *++S = f;                           break;
		case DROP:    cd(1); f = *S--;                           break;
		case OVER:    cd(2); w = *S; *++S = f; f = w;            break;
		case TAIL:
			m[RSTK]--;
			break;
		case BSAVE:
			cd(2);
			f = blockio(*S--, f, 'w');
			break;
		case BLOAD:
			cd(2);
			f = blockio(*S--, f, 'r');
			break;
		case FIND:
			*++S = f;
			if(forth_get_word(o.s) < 0)
				goto end;
			f = forth_find((char*)o.s);
			f = f < DICTIONARY_START ? 0 : f;
			break;
		case DEPTH:
			w = S - o.vstart;
			*++S = f;
			f = w;
			break;
		case CLOCK:
			*++S = f;
			f = ((1000 * clock()) - clk) / CLOCKS_PER_SEC;
			break;
		case EVALUATE:
		{ 
			/* save current input */
			forth_cell_t sin    = o.m[SIN],  sidx = o.m[SIDX],
				slen   = o.m[SLEN], fin  = o.m[FIN],
				source = o.m[SOURCE_ID], r = m[RSTK];
			cd(2);
			char *s = forth_get_string(&S, f);
			f = *S--;
			/* save the stack variables */
			o.S = S;
			o.m[TOP] = f;
			/* push a fake call to forth_eval */
			m[RSTK]++;
			w = forth_eval(s);
			/* restore stack variables */
			m[RSTK] = r;
			S = o.S;
			*++S = o.m[TOP];
			f = w;
			/* restore input stream */
			o.m[SIN]  = sin;
			o.m[SIDX] = sidx;
			o.m[SLEN] = slen;
			o.m[FIN]  = fin;
			o.m[SOURCE_ID] = source;
			if(o.m[INVALID])
				return -1;
		}
		break;
		case PSTK:    print_stack((FILE*)(o.m[STDOUT]), S, f);   break;
		case RESTART: cd(1); longjmp(on_error, f);               break;
		case HELP:    help();                                    break;
		default:
			fatal("illegal operation %" PRIdCell, w);
			longjmp(on_error, FATAL);
		}
	}
end:	o.S = S;
	o.m[TOP] = f;
	return 0;
}

int main(void)
{
	fputs("STATIC FORTH: TYPE 'HELP' FOR BASIC INFORMATION\n", stderr);
	forth_init(stdin, stdout);
	return forth_run();
}

