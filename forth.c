/**
@file       forth.c
@author     Richard James Howe.
@copyright  Copyright 2015,2016 Richard James Howe.
@email      howe.r.j.89@gmail.com

@brief      A FORTH as a kernel module

This file implements the core Forth interpreter, as a Linux kernel module. 

The file contains a virtual machine that can interpret threaded Forth
code and a simple compiler for the virtual machine, which is one of its
instructions. 

@todo either setup an ioctrl, or create a file in "/sys", to control and 
display information about the forth interpreter.

Information to display in "/sys":

Output:
* cycle count
* error log
* help message
* version information

Input:
* max cycle count to run interpreter for

See:
* https://stackoverflow.com/questions/15807846/ioctl-linux-device-driver
* https://www.kernel.org/doc/Documentation/filesystems/sysfs.txt 

**/

#include <linux/init.h>   
#include <linux/module.h> 
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <stdarg.h>

#define  DEVICE_NAME "forth"
#define  CLASS_NAME  "forth"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Howe");
MODULE_DESCRIPTION("A Forth interpreter as a device");
MODULE_VERSION("0.1");

typedef uintptr_t forth_cell_t; /**< FORTH cell large enough for a pointer*/

static int logger(const char *prefix, const char *func, 
		unsigned line, const char *fmt, ...);
static int forth_run(void); 
static ssize_t forth_info_show(struct device *dev, struct device_attribute *attr, char *buf);
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

#define CORE_SIZE (4096)
/* linux requires: sizeof(void*) == sizeof(long) ??? */
#define PRIdCell "ld" /**< Decimal format specifier for a Forth cell */
#define PRIxCell "lx" /**< Hex format specifier for a Forth word */

#define fatal(FMT,...)   logger(KERN_ALERT,  __func__, __LINE__, FMT, ##__VA_ARGS__)
#define error(FMT,...)   logger(KERN_CRIT,   __func__, __LINE__, FMT, ##__VA_ARGS__)
#define warning(FMT,...) logger(KERN_ERR,    __func__, __LINE__, FMT, ##__VA_ARGS__)
#define note(FMT,...)    logger(KERN_INFO,   __func__, __LINE__, FMT, ##__VA_ARGS__)
#define debug(FMT,...)   logger(KERN_DEBUG,  __func__, __LINE__, FMT, ##__VA_ARGS__)

#ifndef NDEBUG
#define ck(C) check_bounds((C), __LINE__, CORE_SIZE)
#define ckchar(C) check_bounds((C), __LINE__, \
			CORE_SIZE * sizeof(forth_cell_t))
#define cd(DEPTH) check_depth(S, (DEPTH), __LINE__)
#define dic(DPTR) check_dictionary((DPTR))
#else
#define ck(C) (C)
#define ckchar(C) (C)
#define cd(I_DEPTH) ((void)I_DEPTH)
#define dic(DPTR) check_dictionary((DPTR))
#endif

#define DEFAULT_CORE_SIZE   (32 * 1024) 
#define STRING_OFFSET       (32u)
#define MAXIMUM_WORD_LENGTH (32u)
#define MINIMUM_STACK_SIZE  (64u)
#define DICTIONARY_START (STRING_OFFSET+MAXIMUM_WORD_LENGTH) /**< start of dic*/
#define WORD_LENGTH_OFFSET  (8)  
#define WORD_LENGTH(MISC) (((MISC) >> WORD_LENGTH_OFFSET) & 0xff)
#define WORD_HIDDEN(MISC) ((MISC) & 0x80)
#define INSTRUCTION_MASK    (0x7f)
#define instruction(k)      ((k) & INSTRUCTION_MASK)
#define IS_BIG_ENDIAN (!(union { uint16_t u16; uint8_t c; }){ .u16 = 1 }.c)
#define CORE_VERSION        (0x02u)
#define MAX_BUFFER_LENGTH   (256)
#define FORTH_VERSION       "2"

/** @todo add more words to the startup program **/
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
": cr 10 emit ; \n"
": . pnum drop space ; \n";

static const char conv[] = "0123456789abcdefghijklmnopqrstuvwxzy";
static struct forth o;


static int    major_number;
static char   input[MAX_BUFFER_LENGTH] = {0};
static short  input_count;
static unsigned long cycle_counter = 0;

static char   output[MAX_BUFFER_LENGTH] = {0};
static short  output_index;

static int    open_count = 0;
static struct class*  class  = NULL;
static struct device* device = NULL;

static DEFINE_MUTEX(forth_mutex);
static DEVICE_ATTR(info, S_IRUGO, forth_info_show, NULL);

static struct file_operations fops =
{
	.open     =  dev_open,
	.read     =  dev_read,
	.write    =  dev_write,
	.release  =  dev_release,
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
	SIN         = 11, /**< string input pointer */
	SIDX        = 12, /**< string input index */
	SLEN        = 13, /**< string input length */
	START_ADDR  = 14, /**< pointer to start of VM */
	DEBUG       = 15, /**< turn debugging on/off if enabled */
	INVALID     = 16, /**< if non zero, this interpreter is invalid */
	TOP         = 17, /**< *stored* version of top of stack */
	INSTRUCTION = 18, /**< start up instruction */
	STACK_SIZE  = 19, /**< size of the stacks */
	ERROR_HANDLER = 20, /**< actions to take on error */
};

static const char *register_names[] = { "h", "r", "`state", "base", "pwd",
"`sin", "`sidx", "`slen", "`start-address", "`debug", "`invalid", 
"`top", "`instruction", "`stack-size", "`error-handler", NULL };

#define XMACRO_INSTRUCTIONS\
 X(I_PUSH,      "push",       " -- x : push a literal")\
 X(I_COMPILE,   "compile",    " -- : compile a pointer to a Forth word")\
 X(I_RUN,       "run",        " -- : run a Forth word")\
 X(I_DEFINE,    "define",     " -- : make new Forth word, set compile mode")\
 X(I_IMMEDIATE, "immediate",  " -- : make a Forth word immediate")\
 X(I_READ,      "read",       " -- : read in a Forth word and execute it")\
 X(I_LOAD,      "@",          "addr -- x : load a value")\
 X(I_STORE,     "!",          "x addr -- : store a value")\
 X(I_CLOAD,     "c@",         "c-addr -- x : load character value")\
 X(I_CSTORE,    "c!",         "x c-addr -- : store character value")\
 X(I_SUB,       "-",          "x1 x2 -- x3 : subtract x2 from x1 yielding x3")\
 X(I_ADD,       "+",          "x x -- x : add two values")\
 X(I_AND,       "and",        "x x -- x : bitwise and of two values")\
 X(I_OR,        "or",         "x x -- x : bitwise or of two values")\
 X(I_XOR,       "xor",        "x x -- x : bitwise exclusive or of two values")\
 X(I_INV,       "invert",     "x -- x : invert bits of value")\
 X(I_SHL,       "lshift",     "x1 x2 -- x3 : left shift x1 by x2")\
 X(I_SHR,       "rshift",     "x1 x2 -- x3 : right shift x1 by x2")\
 X(I_MULTIPLY,  "*",          "x x -- x : multiply to values")\
 X(I_DIVIDE,    "/",          "x1 x2 -- x3 : divide x1 by x2 yielding x3")\
 X(I_ULESS,     "u<",         "x x -- bool : unsigned less than")\
 X(I_UMORE,     "u>",         "x x -- bool : unsigned greater than")\
 X(I_EXIT,      "exit",       " -- : return from a word defition")\
 X(I_KEY,       "key",        " -- char : get one character of input")\
 X(I_EMIT,      "_emit",      "char -- bool : emit one character to output")\
 X(I_FROMR,     "r>",         " -- x, R: x -- : move from return stack")\
 X(I_TOR,       ">r",         "x --, R: -- x : move to return stack")\
 X(I_BRANCH,    "branch",     " -- : unconditional branch")\
 X(I_QBRANCH,   "?branch",    "x -- : branch if x is zero")\
 X(I_PNUM,      "pnum",       "x -- : print a number")\
 X(I_QUOTE,     "'",          " -- addr : push address of word")\
 X(I_COMMA,     ",",          "x -- : write a value into the dictionary")\
 X(I_EQUAL,     "=",          "x x -- bool : compare two values for equality")\
 X(I_SWAP,      "swap",       "x1 x2 -- x2 x1 : swap two values")\
 X(I_DUP,       "dup",        "x -- x x : duplicate a value")\
 X(I_DROP,      "drop",       "x -- : drop a value")\
 X(I_OVER,      "over",       "x1 x2 -- x1 x2 x1 : copy over a value")\
 X(I_TAIL,      "tail",       " -- : tail recursion")\
 X(I_FIND,      "find",       "c\" xxx\" -- addr | 0 : find a Forth word")\
 X(I_DEPTH,     "depth",      " -- x : get current stack depth")\
 X(I_CLOCK,     "clock",      " -- x : push a time value")\
 X(I_PSTK,      ".s",         " -- : print out values on the stack")\
 X(I_RESTART,   "restart",    " error -- : restart system, cause error")\
 X(LAST_INSTRUCTION, NULL, "")

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

static int logger(const char *prefix, const char *func, 
		unsigned line, const char *fmt, ...)
{
	int r;
	va_list ap;
	printk("%s[FORTH-LKM %s %u]: ", prefix, func, line);
	va_start(ap, fmt);
	r = vprintk(fmt, ap);
	va_end(ap);
	printk("\n");
	return r;
}

static int dputc(char c)
{
	if(output_index >= MAX_BUFFER_LENGTH - 1)
		return -1;
	output[output_index++] = c;
	return c;
}

static int dprintf(const char *fmt, ...)
{
	int r = 0;
	va_list ap;
	if(output_index >= MAX_BUFFER_LENGTH - 1)
		return -1;
	va_start(ap, fmt);
	r = vsnprintf(output, (MAX_BUFFER_LENGTH - 1) - output_index, fmt, ap);
	va_end(ap);
	if(r > 0)
		output_index += r;
	return r;
}

static int forth_get_char(void)
{
	return o.m[SIDX] >= o.m[SLEN] ? -1 : ((char*)(o.m[SIN]))[o.m[SIDX]++];
}

static int forth_get_word(uint8_t *p)
{
	int n = 0;
	if(sscanf((char *)&(((char*)(o.m[SIN]))[o.m[SIDX]]), o.word_fmt, p, &n) <= 0)
		return -1;
	if(!((char*)o.s)[0])
		return -1;
	o.m[SIDX] += n;
	return n;
}

static void compile(forth_cell_t code, const char *str)
{ 
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

static int print_unsigned_number(forth_cell_t u, forth_cell_t base)
{
	int i = 0, r = 0;
	char s[64 + 1] = ""; 
	do 
		s[i++] = conv[u % base];
	while ((u /= base));
	for(; i >= 0 && r >= 0; i--)
		r = dputc(s[i]);
	return r;
}

static int print_cell(forth_cell_t f)
{
	unsigned base = o.m[BASE];
	if(base == 10 || base == 0)
		return dprintf("%"PRIdCell, f);
	if(base == 16)
		return dprintf(o.hex_fmt, f);
	if(base == 1 || base > 36)
		return -1;
	return print_unsigned_number(f, base);
}

static forth_cell_t check_bounds(forth_cell_t f, unsigned line, forth_cell_t bound)
{
	if(o.m[DEBUG] >= DEBUG_CHECKS)
		debug("0x%"PRIxCell " %u", f, line);
	if(f >= bound) {
		fatal("bounds check failed (%" PRIdCell " >= %zu)", f, (size_t)bound);
		o.m[INVALID] = 1;
		return 0;
	}
	return f;
}

static void check_depth(forth_cell_t *S, forth_cell_t expected, unsigned line)
{
	if(o.m[DEBUG] >= DEBUG_CHECKS)
		debug("0x%"PRIxCell " %u", (forth_cell_t)(S - o.vstart), line);
	if((uintptr_t)(S - o.vstart) < expected) {
		error("stack underflow %p", S);
		o.m[INVALID] = 1;
	} else if(S > o.vend) {
		error("stack overflow %p", S - o.vend);
		o.m[INVALID] = 1;
	}
}

static forth_cell_t check_dictionary(forth_cell_t dptr)
{
	if((o.m + dptr) >= (o.vstart)) {
		fatal("dictionary pointer is in stack area %"PRIdCell, dptr);
		o.m[INVALID] = 1;
		return 0;
	}
	return dptr;
}

static void print_stack(forth_cell_t *S, forth_cell_t f)
{ 
	forth_cell_t depth = (forth_cell_t)(S - o.vstart);
	dprintf("%"PRIdCell": ", depth);
	if(!depth)
		return;
	print_cell(f);
	dputc(' ');
	while(o.vstart + 1 < S) {
		print_cell(*(S--));
		dputc(' ');
	}
	dputc('\n');
}

static void forth_set_string_input(const char *s, size_t length)
{
	o.m[SIDX] = 0;              /* m[SIDX] == current character in string */
	o.m[SLEN] = length;         /* m[SLEN] == string len */
	o.m[SIN] = (forth_cell_t)s; /* sin  == pointer to string input */
}

static int forth_eval(const char *s)
{
	forth_set_string_input(s, strlen(s) + 1);
	return forth_run();
}

static int forth_eval_block(const char *s, size_t length)
{
	forth_set_string_input(s, length);
	return forth_run();
}

static int forth_define_constant(const char *name, forth_cell_t c)
{
	char e[MAXIMUM_WORD_LENGTH+32] = {0};
	sprintf(e, ": %31s %" PRIdCell " ; \n", name, c);
	return forth_eval(e);
}

static void forth_make_default(void)
{
	o.m[STACK_SIZE] = CORE_SIZE / MINIMUM_STACK_SIZE > MINIMUM_STACK_SIZE ?
				CORE_SIZE / MINIMUM_STACK_SIZE :
				MINIMUM_STACK_SIZE;

	o.s             = (uint8_t*)(o.m + STRING_OFFSET); /*skip registers*/
	o.m[START_ADDR] = (forth_cell_t)&(o.m);
	o.m[RSTK] = CORE_SIZE - o.m[STACK_SIZE]; /* set up return stk ptr */
	o.S       = o.m + CORE_SIZE - (2 * o.m[STACK_SIZE]); /* v. stk pointer */
	o.vstart  = o.m + CORE_SIZE - (2 * o.m[STACK_SIZE]);
	o.vend    = o.vstart + o.m[STACK_SIZE];
	sprintf(o.hex_fmt, "0x%%0%d"PRIxCell, (int)sizeof(forth_cell_t)*2);
	sprintf(o.word_fmt, "%%%ds%%n", MAXIMUM_WORD_LENGTH - 1);
}

static int forth_core_init(void)
{
	forth_cell_t *m, i, w, t;

	note("initializing forth core");

	forth_make_default();

	m = o.m;       /*a local variable only for convenience */

	o.m[PWD]   = 0;  /* special terminating pwd value */
	t = m[DIC] = DICTIONARY_START; /* initial dictionary offset */
	m[m[DIC]++] = I_TAIL; /* add a I_TAIL instruction that can be called */
	w = m[DIC];         /* save current offset, which will contain I_READ */
	m[m[DIC]++] = I_READ; /* populate the cell with I_READ */
	m[m[DIC]++] = I_RUN;  /* call the special word recursively */
	o.m[INSTRUCTION] = m[DIC]; /* stream points to the special word */
	m[m[DIC]++] = w;    /* call to I_READ word */
	m[m[DIC]++] = t;    /* call to I_TAIL */
	m[m[DIC]++] = o.m[INSTRUCTION] - 1; /* recurse*/

	debug("compiling first words");

	compile(I_DEFINE,    ":");
	compile(I_IMMEDIATE, "immediate");

	for(i = I_READ, w = I_READ; instruction_names[i]; i++) {
		compile(I_COMPILE, instruction_names[i]);
		m[m[DIC]++] = w++; /*This adds the actual VM instruction */
	}

	debug("first evaluation");

	if(forth_eval(": state 8 exit : ; immediate ' exit , 0 state ! ;") < 0)
		return -1;

	debug("defining constants");

	for(i = 0; register_names[i]; i++)
		if(forth_define_constant(register_names[i], i+DIC) < 0)
			return -1;

	if(forth_define_constant("size", sizeof(forth_cell_t)) < 0)
		return -1;

	if(forth_define_constant("stack-start", CORE_SIZE - (2 * o.m[STACK_SIZE])) < 0)
		return -1;

	if(forth_define_constant("max-core", CORE_SIZE) < 0)
		return -1;

	if(forth_define_constant("dictionary-start",  DICTIONARY_START) < 0)
		return -1;

	if(forth_define_constant(">in",  STRING_OFFSET * sizeof(forth_cell_t)))
		return -1;

	debug("evaluating startup program");

	if(forth_eval(initial_forth_program) < 0)
		return -1;

	debug("finished forth core initialization successfully");
	return 0;
}

static int forth_run(void)
{
	forth_cell_t *m = o.m, pc, *S = o.S, I = o.m[INSTRUCTION], f = o.m[TOP], w;

	if(o.m[INVALID]) {
		fatal("refusing to run an invalid forth, %"PRIdCell, o.m[INVALID]);
		return -1;
	}

restart:
	
	for(;(pc = m[ck(I++)]);) { 
	INNER:  
		cycle_counter++;
		if(o.m[INVALID])
			return -1;
		w = instruction(m[ck(pc++)]);
		switch (w) { 
		case I_PUSH:    *++S = f;     f = m[ck(I++)];          break;
		case I_COMPILE: m[dic(m[DIC]++)] = pc;                 break; 
		case I_RUN:     m[ck(++m[RSTK])] = I; I = pc;          break;
		case I_DEFINE:
			m[STATE] = 1; /* compile mode */
			if(forth_get_word(o.s) < 0)
				goto end;
			compile(I_COMPILE, (char*)o.s);
			m[dic(m[DIC]++)] = I_RUN;
			break;
		case I_IMMEDIATE:
			m[DIC] -= 2; /* move to first code field */
			m[m[DIC]] &= ~INSTRUCTION_MASK; /* zero instruction */
			m[m[DIC]] |= I_RUN; /* set instruction to I_RUN */
			dic(m[DIC]++); /* compilation start here */ 
			break;
		case I_READ:
		{
			if(forth_get_word(o.s) < 0)
				goto end;
			/*debug("word: %s", (char*)(o.s));*/
			if ((w = forth_find((char*)o.s)) > 1) {
				pc = w;
				if (!m[STATE] && instruction(m[ck(pc)]) == I_COMPILE)
					pc++; /* in command mode, execute word */
				goto INNER;
			} else if(kstrtol((char*)o.s, o.m[BASE], &w)) {
				error("'%s' is not a word", o.s);
				goto restart;
			}
			if (m[STATE]) { /* must be a number then */
				m[dic(m[DIC]++)] = 2; /*fake word push at m[2] */
				m[dic(m[DIC]++)] = w;
			} else { /* push word */
				*++S = f;
				f = w;
			}
			break;
		}
		case I_LOAD:    cd(1); f = m[ck(f)];                 break;
		case I_STORE:   cd(2); m[ck(f)] = *S--; f = *S--;    break;
		case I_CLOAD:   cd(1); f = *(((uint8_t*)m) + ckchar(f)); break;
		case I_CSTORE:  cd(2); ((uint8_t*)m)[ckchar(f)] = *S--; f = *S--; break;
		case I_SUB:     cd(2); f = *S-- - f;                 break;
		case I_ADD:     cd(2); f = *S-- + f;                 break;
		case I_AND:     cd(2); f = *S-- & f;                 break;
		case I_OR:      cd(2); f = *S-- | f;                 break;
		case I_XOR:     cd(2); f = *S-- ^ f;                 break;
		case I_INV:     cd(1); f = ~f;                       break;
		case I_SHL:     cd(2); f = *S-- << f;                break;
		case I_SHR:     cd(2); f = *S-- >> f;                break;
		case I_MULTIPLY:     cd(2); f = *S-- * f;            break;
		case I_DIVIDE:
			cd(2);
			if(f) {
				f = *S-- / f;
			} else {
				error("divide %"PRIdCell" by zero ", *S--);
				goto restart;
			} 
			break;
		case I_ULESS:   cd(2); f = *S-- < f;                       break;
		case I_UMORE:   cd(2); f = *S-- > f;                       break;
		case I_EXIT:    I = m[ck(m[RSTK]--)];                      break;
		case I_KEY:     *++S = f; f = forth_get_char();            break;
		case I_EMIT:    f = dputc(f);                              break;
		case I_FROMR:   *++S = f; f = m[ck(m[RSTK]--)];            break;
		case I_TOR:     cd(1); m[ck(++m[RSTK])] = f; f = *S--;     break;
		case I_BRANCH:  I += m[ck(I)];                             break;
		case I_QBRANCH: cd(1); I += f == 0 ? m[I] : 1; f = *S--;   break;
		case I_PNUM:    cd(1); 
				f = print_cell(f);                      
				break;
		case I_QUOTE:   *++S = f;     f = m[ck(I++)];              break;
		case I_COMMA:   cd(1); m[dic(m[DIC]++)] = f; f = *S--;     break;
		case I_EQUAL:   cd(2); f = *S-- == f;                      break;
		case I_SWAP:    cd(2); w = f;  f = *S--;   *++S = w;       break;
		case I_DUP:     cd(1); *++S = f;                           break;
		case I_DROP:    cd(1); f = *S--;                           break;
		case I_OVER:    cd(2); w = *S; *++S = f; f = w;            break;
		case I_TAIL:
			m[RSTK]--;
			break;
		case I_FIND:
			*++S = f;
			if(forth_get_word(o.s) < 0)
				goto end;
			f = forth_find((char*)o.s);
			f = f < DICTIONARY_START ? 0 : f;
			break;
		case I_DEPTH:
			w = S - o.vstart;
			*++S = f;
			f = w;
			break;
		case I_CLOCK:
			*++S = f;
			f = jiffies;
			break;
		case I_PSTK:    print_stack(S, f);   break;
		case I_RESTART: cd(1); goto restart; break;
		default:
			fatal("illegal operation %" PRIdCell, w);
			o.m[INVALID] = 1;
			return -1;
		}
	}
end:	o.S = S;
	o.m[TOP] = f;
	return 0;
}

static ssize_t forth_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "FORTH-LKM Version: %s\n", FORTH_VERSION);
}

static int __init forth_init(void)
{
	void *ptr = NULL;
	int rval = 0, file_failed = 0;
	debug("Initializing FORTH-LKM");

	if(forth_core_init() < 0) {
		fatal("failed to initialize forth core");
		return -EINVAL;
	}

	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if(major_number < 0) {
		fatal("failed to register major number: %d", major_number);
		return major_number;
	}

	debug("registered major number: %d", major_number);

	class = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(class)) {
		ptr = class;
		fatal("failed to register device class");
		goto fail;
	}
	debug("register device class");

	device = device_create(class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if(IS_ERR(device)) {
		fatal("failed to create device");
		ptr = device;
		goto fail;
	}

	rval = device_create_file(device, &dev_attr_info);
	if(rval) {
		fatal("failed to created info file");
		file_failed = 1;
		goto fail;
	}

	mutex_init(&forth_mutex);
	debug("module initialized");
	return 0;
fail:
	if(file_failed)
		device_remove_file(device, &dev_attr_info);
	/** @todo destroy device correctly
	if(device)
		device_destroy(class, device);*/
	if(class)
		class_destroy(class);
	if(major_number > 0)
		unregister_chrdev(major_number, DEVICE_NAME);
	return PTR_ERR(ptr);
}

static void __exit forth_exit(void)
{
	mutex_destroy(&forth_mutex);
	device_remove_file(device, &dev_attr_info);
	device_destroy(class, MKDEV(major_number, 0));
	class_unregister(class);
	class_destroy(class);
	unregister_chrdev(major_number, DEVICE_NAME);
	note("unregistered device");
}

static int dev_open(struct inode *inodep, struct file *filep) 
{
	if(!mutex_trylock(&forth_mutex)) {
		error("Device is busy or in use by another process");
		return -EBUSY;
	}
	open_count++;
	note("opened %d times", open_count);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	/**@todo blocking on no data? */
	int err = 0;
	short rval = output_index;
	err = copy_to_user(buffer, output, output_index);
	output_index = 0;
	if(err == 0) {
		debug("sent %hd chars", rval);
		return rval;
	} else {
		error("failed to send %d chars to user", output_index);
		return -EINVAL;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	int r = 0;
	if(len > MAX_BUFFER_LENGTH-1) {
		debug("write to large (%zu > %d)", len, MAX_BUFFER_LENGTH-1);
		return -EINVAL;
	}
	input_count = snprintf(input, MAX_BUFFER_LENGTH-1, "%s", buffer);
	debug("received %zu chars: %s", len, input);
	if(input_count > 0) {
		debug("running forth interpreter");
		r = forth_eval_block(input, len);
		if(r < 0) {
			error("forth_eval_block returned: %d", r);
			/**@todo reinitialize if invalid*/
			return -EINVAL;
		} else {
			debug("interpreter ran for %ld cycles, wrote %hd", cycle_counter, output_index);
		}
	}

	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	mutex_unlock(&forth_mutex);
	note("device closed");
	return 0;
}

module_init(forth_init);
module_exit(forth_exit);


