/** @file       libforth.c
 *  @brief      A FORTH library, based on <www.ioccc.org/1992/buzzard.2.c>
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015,2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  Please consult "readme.md", "forth.fth" and "libforth.h" for more information 
 *  @todo add a system for adding arbitrary C functions to the system via
 *  plugins 
 *  @todo Add file access utilities
 *  @todo Turn this file into a literate-style document, as in Jonesforth**/
#include "libforth.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>

#ifndef NDEBUG
#define ck(c) check_bounds(o, c, __LINE__)
#else
#define ck(c) (c) /*disables checks and debug mode*/
#endif

#define DEFAULT_CORE_SIZE   (32 * 1024) /**< default vm size*/
#define BLOCK_SIZE          (1024u) /**< size of forth block in bytes */
#define STRING_OFFSET       (32u)   /**< offset into memory of string buffer*/
#define MAX_WORD_LENGTH     (32u)   /**< max word length, must be < 255 */
#define DICTIONARY_START    (STRING_OFFSET + MAX_WORD_LENGTH) /**< start of dic */
#define WORD_LENGTH_OFFSET  (8)  /**< bit offset for word length start*/
#define WORD_LENGTH(FIELD1) (((FIELD1) >> WORD_LENGTH_OFFSET) & 0xff)
#define WORD_HIDDEN(FIELD1) ((FIELD1) & 0x80) /**< is a forth word hidden? */
#define INSTRUCTION_MASK    (0x7f)
#define instruction(k)      ((k) & INSTRUCTION_MASK)
#define VERIFY(X)           do { if(!(X)) { abort(); } } while(0)
#define IS_BIG_ENDIAN       (!(union { uint16_t u16; unsigned char c; }){ .u16 = 1 }.c)
#define CORE_VERSION        (0x02u) /**< version of the forth core file */

static const char *initial_forth_program = "\n\
: here h @ ; : [ immediate 0 state ! ; : ] 1 state ! ; : >mark here 0 , ;      \n\
: :noname immediate -1 , here 2 , ] ; : if immediate ' ?branch , >mark ;       \n\
: else immediate ' branch , >mark swap dup here swap - swap ! ; : 0= 0 = ;     \n\
: then immediate dup here swap - swap ! ; : 2dup over over ; : <> = 0= ;       \n\
: begin immediate here ; : until immediate ' ?branch , here - , ; : '\\n' 10 ; \n\
: not 0= ; : 1+ 1 + ; : 1- 1 - ; : ')' 41 ; : tab 9 emit ; : cr '\\n' emit ;   \n\
: ( immediate begin key ')' = until ; : rot >r swap r> swap ; : -rot rot rot ; \n\
: tuck swap over ; : nip swap drop ; : :: [ find : , ] ; : allot here + h ! ; ";

enum header { MAGIC0, MAGIC1, MAGIC2, MAGIC4, CELL_SIZE, VERSION, ENDIAN, MAGIC7 };
static const uint8_t header[MAGIC7+1] = {
	0xFF, '4','T','H',    /* file magic number */
	sizeof(forth_cell_t), /* size is needed for parsing rest of header if dumped */
	CORE_VERSION,         /* version of forth core*/
	-1,                   /* endianess of platform, filled in later*/
	0xFF                  /* end header */
};

struct forth { /**< FORTH environment, values marked '~~' are serialized in order*/
	uint8_t header[sizeof(header)]; /**< ~~ header for reloadable core file */
	forth_cell_t core_size;  /**< ~~ size of VM (converted to uint64_t for serialization)*/
	jmp_buf *on_error;   /**< place to jump to on error */
	uint8_t *s;          /**< convenience pointer for string input buffer */
	char hex_fmt[16];    /**< calculated hex format*/
	forth_cell_t *S;     /**< stack pointer */
	forth_cell_t m[];    /**< ~~ Forth Virtual Machine memory */
};

enum registers {          /**< virtual machine registers */
	DIC         =  6, /**< dictionary pointer */
	RSTK        =  7, /**< return stack pointer */
	STATE       =  8, /**< interpreter state; compile or command mode*/
	BASE        =  9, /**< base conversion variable */
	PWD         = 10, /**< pointer to previous word */
	SOURCE_ID   = 11, /**< input source selector */
	SIN         = 12, /**< string input pointer*/
	SIDX        = 13, /**< string input index*/
	SLEN        = 14, /**< string input length*/ 
	START_ADDR  = 15, /**< pointer to start of VM */
	FIN         = 16, /**< file input pointer */
	FOUT        = 17, /**< file output pointer */
	STDIN       = 18, /**< file pointer to stdin */
	STDOUT      = 19, /**< file pointer to stdout */
	STDERR      = 20, /**< file pointer to stderr */
	ARGC        = 21, /**< argument count */
	ARGV        = 22, /**< arguments */
	DEBUG       = 23, /**< turn debugging on/off if enabled*/
	INVALID     = 24, /**< if non zero, this interpreter is invalid */
	TOP         = 25, /**< *stored* version of top of stack */
	INSTRUCTION = 26, /**< *stored* version of instruction pointer*/
	STACK_SIZE  = 27, /**< size of the stacks */
	START_TIME  = 28, /**< start time in milliseconds */
};

enum input_stream { FILE_IN, STRING_IN = -1 };

static const char *register_names[] = { "h", "r", "`state", "base", "pwd",
"`source-id", "`sin", "`sidx", "`slen", "`start-address", "`fin", "`fout", "`stdin",
"`stdout", "`stderr", "`argc", "`argv", "`debug", "`invalid", "`top", "`instruction",
"`stack-size", "`start-time", NULL };

enum instructions { PUSH,COMPILE,RUN,DEFINE,IMMEDIATE,READ,LOAD,STORE,
SUB,ADD,AND,OR,XOR,INV,SHL,SHR,MUL,DIV,LESS,MORE,EXIT,EMIT,KEY,FROMR,TOR,BRANCH,
QBRANCH, PNUM, QUOTE,COMMA,EQUAL,SWAP,DUP,DROP,OVER,TAIL,BSAVE,BLOAD,FIND,PRINT,
DEPTH,CLOCK,LAST }; /* all virtual machine instructions */

static const char *instruction_names[] = { "read","@","!","-","+","and","or",
"xor","invert","lshift","rshift","*","/","u<","u>","exit","emit","key","r>",
">r","branch","?branch", "pnum","'", ",","=", "swap","dup","drop", "over", "tail",
"bsave","bload", "find", "print","depth","clock", NULL }; /* named VM instructions */

static int forth_get_char(forth_t *o) 
{ /* get a char from string input or a file */
	switch(o->m[SOURCE_ID]) {
	case FILE_IN:   return fgetc((FILE*)(o->m[FIN]));
	case STRING_IN: return o->m[SIDX] >= o->m[SLEN] ? EOF : ((char*)(o->m[SIN]))[o->m[SIDX]++];
	default:        return EOF;
	}
} 

static int forth_get_word(forth_t *o, uint8_t *p) 
{ /*get a word (space delimited, up to 31 chars) from a FILE* or string-in*/ 
	int n = 0;
	char fmt[16] = { 0 };
	sprintf(fmt, "%%%ds%%n", MAX_WORD_LENGTH - 1);
	switch(o->m[SOURCE_ID]) {
	case FILE_IN:   return fscanf((FILE*)(o->m[FIN]), fmt, p, &n);
	case STRING_IN:
		if(sscanf((char *)&(((char*)(o->m[SIN]))[o->m[SIDX]]), fmt, p, &n) < 0)
			return EOF;
		return o->m[SIDX] += n, n;
	default:       return EOF;
	}
} 

static void compile(forth_t *o, forth_cell_t code, const char *str) 
{ /* create a new forth word header */
	assert(o && code < LAST);
	forth_cell_t *m = o->m, header = m[DIC], l = 0;
	/*FORTH header structure*/
	strcpy((char *)(o->m + header), str); /* 0: Copy the new FORTH word into the new header */
	l = strlen(str) + 1;
	l = (l + (sizeof(forth_cell_t) - 1)) & ~(sizeof(forth_cell_t) - 1); /* align up to sizeof word */
	l = l/sizeof(forth_cell_t);
	m[DIC] += l; /* Add string length in words to header (STRLEN) */

	m[m[DIC]++] = m[PWD];     /*0 + STRLEN: Pointer to previous words header*/
	m[PWD] = m[DIC] - 1;      /*   Update the PWD register to new word */
	m[m[DIC]++] = (l << WORD_LENGTH_OFFSET) | code; /*1: size of words name and code field */
}

static int blockio(forth_t *o, void *p, forth_cell_t poffset, forth_cell_t id, char rw) 
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
	n = rw == 'w' ? fwrite(((char*)p) + poffset, 1, BLOCK_SIZE, file):
			fread (((char*)p) + poffset, 1, BLOCK_SIZE, file);
	fclose(file);
	return n == BLOCK_SIZE ? 0 : -1;
} /*a basic FORTH block I/O interface*/

static int numberify(int base, forth_cell_t *n, const char *s)  
{ /*returns non zero if conversion was successful*/
	char *end = NULL;
	errno = 0;
	*n = strtol(s, &end, base);
	return !errno && *s != '\0' && *end == '\0';
}

forth_cell_t forth_find(forth_t *o, const char *s) 
{ /* find a word in the Forth dictionary, which is a linked list, skipping hidden words */
	forth_cell_t *m = o->m, w = m[PWD], len = WORD_LENGTH(m[w+1]);
	for (;w > DICTIONARY_START && (WORD_HIDDEN(m[w+1]) || strcmp(s,(char*)(&o->m[w - len])));) {
		w = m[w]; 
		len = WORD_LENGTH(m[w+1]);
	}
	return w > DICTIONARY_START ? w+1 : 0;
}

static int print_cell(forth_t *o, forth_cell_t f)
{ 
	char *fmt = o->m[BASE] == 16 ? o->hex_fmt : "%" PRIuCell;
	return fprintf((FILE*)(o->m[FOUT]), fmt, f);
}

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
	o->m[SIDX] = 0;              /*m[SIDX] == current character in string*/
	o->m[SLEN] = strlen(s) + 1;  /*m[SLEN] == string len*/
	o->m[SOURCE_ID] = STRING_IN; /*read from string, not a file handle*/
	o->m[SIN] = (forth_cell_t)s; /*sin  == pointer to string input*/
}

int forth_eval(forth_t *o, const char *s) 
{ 
	assert(o && s); 
	forth_set_string_input(o, s); 
	return forth_run(o);
}

int forth_define_constant(forth_t *o, const char *name, forth_cell_t c)
{
	char e[MAX_WORD_LENGTH+32] = {0};
	assert(o && strlen(name) < MAX_WORD_LENGTH);
	sprintf(e, ": %31s %" PRIuCell " ; \n", name, c);
	return forth_eval(o, e);
}

static void forth_make_default(forth_t *o, size_t size, FILE *in, FILE *out)
{ /* set defaults for a forth structure for initialization or reload */
	o->core_size     = size;
	o->m[STACK_SIZE] = size / 64 > 64 ? size / 64 : 64;
	o->s             = (uint8_t*)(o->m + STRING_OFFSET); /*string store offset into CORE, skip registers*/
	o->m[FOUT]       = (forth_cell_t)out;
	o->m[START_ADDR] = (forth_cell_t)&(o->m);
	o->m[STDIN]      = (forth_cell_t)stdin;
	o->m[STDOUT]     = (forth_cell_t)stdout;
	o->m[STDERR]     = (forth_cell_t)stderr;
	o->m[RSTK]       = size - o->m[STACK_SIZE];     /*set up return stk pointer*/
	o->m[START_TIME] = (1000 * clock()) / CLOCKS_PER_SEC;
	o->m[ARGC] = o->m[ARGV] = 0;
	o->S             = o->m + size - (2 * o->m[STACK_SIZE]); /*set up variable stk pointer*/
	sprintf(o->hex_fmt, "0x%%0%d"PRIxCell, (int)(sizeof(forth_cell_t)*2));
	o->on_error      = calloc(sizeof(jmp_buf), 1);
	forth_set_file_input(o, in);  /*set up input after our eval*/
}

static void make_header(uint8_t *dst)
{
	memcpy(dst, header, sizeof(header));
	dst[ENDIAN] = !IS_BIG_ENDIAN; /*fill in endianess*/
}

forth_t *forth_init(size_t size, FILE *in, FILE *out) 
{ 
	assert(in && out);
	forth_cell_t *m, i, w, t;
	forth_t *o;
	VERIFY(size >= MINIMUM_CORE_SIZE);
	if(!(o = calloc(1, sizeof(*o) + sizeof(forth_cell_t)*size))) 
		return NULL;
	forth_make_default(o, size, in, out);
	make_header(o->header);
	m = o->m;       /*a local variable only for convenience*/

	/* The next section creates a word that calls READ, then TAIL, then itself*/
	o->m[PWD]   = 0;  /*special terminating pwd value*/
	t = m[DIC] = DICTIONARY_START; /*initial dictionary offset, skip registers and string offset*/
	m[m[DIC]++] = TAIL; /*Add a TAIL instruction that can be called*/
	w = m[DIC];         /*Save current offset, it will contain the READ instruction */
	m[m[DIC]++] = READ; /*create a special word that reads in FORTH*/
	m[m[DIC]++] = RUN;  /*call the special word recursively*/
	o->m[INSTRUCTION] = m[DIC]; /*instruction stream points to our special word*/
	m[m[DIC]++] = w;    /*call to READ word*/
	m[m[DIC]++] = t;    /*call to TAIL*/
	m[m[DIC]++] = o->m[INSTRUCTION] - 1; /*recurse*/

	compile(o, DEFINE,    ":");         /*immediate word*/
	compile(o, IMMEDIATE, "immediate"); /*immediate word*/
	for(i = 0, w = READ; instruction_names[i]; i++) /*compiling words*/
		compile(o, COMPILE, instruction_names[i]), m[m[DIC]++] = w++;
	/* the next eval is the absolute minimum needed for a sane environment */
	VERIFY(forth_eval(o, ": state 8 exit : ; immediate ' exit , 0 state ! ;") >= 0);
	for(i = 0; register_names[i]; i++) /* name all registers */ 
		VERIFY(forth_define_constant(o, register_names[i], i+DIC) >= 0);
	VERIFY(forth_eval(o, initial_forth_program) >= 0);
	VERIFY(forth_define_constant(o, "size",          sizeof(forth_cell_t)) >= 0);
	VERIFY(forth_define_constant(o, "stack-start",   size - (2 * o->m[STACK_SIZE])) >= 0);
	VERIFY(forth_define_constant(o, "max-core",      size) >= 0);

	forth_set_file_input(o, in);  /*set up input after our eval*/
	return o;
}

int forth_dump_core(forth_t *o, FILE *dump) 
{ 
	assert(o && dump);
	size_t w = sizeof(*o) + sizeof(forth_cell_t) * o->core_size;
	return w != fwrite(o, 1, w, dump) ? -1: 0; 
}

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

void forth_free(forth_t *o) 
{ 
	assert(o); 
	free(o->on_error);
	free(o); 
}

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
	return o->S - (o->m + o->core_size - (2 * o->m[STACK_SIZE]));
}

static forth_cell_t check_bounds(forth_t *o, forth_cell_t f, unsigned line) 
{
	if(o->m[DEBUG])
		fprintf(stderr, "\t( debug\t0x%" PRIxCell "\t%u )\n", f, line);
	if(((forth_cell_t)f) >= o->core_size) {
		fprintf(stderr, "( fatal \"bounds check failed: %" PRIuCell " >= %zu\" )\n", f, (size_t)o->core_size);
		longjmp(*o->on_error, 1);
	}
	return f; 
}

int forth_run(forth_t *o) 
{ /* Forth virtual machine; it does the work, everything else is just fluff*/ 
	assert(o);
	if(o->m[INVALID] || setjmp(*o->on_error))
		return -(o->m[INVALID] = 1);
	forth_cell_t *m = o->m, pc, *S = o->S, I = o->m[INSTRUCTION], f = o->m[TOP], w;

	for(;(pc = m[ck(I++)]);) { /* Threaded code interpreter */
	INNER:  assert((S > m) && (S < (m + o->core_size)));
		switch (w = instruction(m[ck(pc++)])) {
		case PUSH:    *++S = f;     f = m[ck(I++)];          break;
		case COMPILE: m[ck(m[DIC]++)] = pc;                  break; /* this needs to be moved into READ */
		case RUN:     m[ck(++m[RSTK])] = I; I = pc;          break;
		case DEFINE:  m[STATE] = 1; /* compile mode */
                              if(forth_get_word(o, (uint8_t*)(o->s)) < 0)
                                      goto end;
                              compile(o, COMPILE, (char*)o->s); 
                              m[ck(m[DIC]++)] = RUN;                 break;
		case IMMEDIATE: 
			      m[DIC] -= 2; /* move to first code field */
			      m[m[DIC]] &= ~INSTRUCTION_MASK; /* zero instruction */
			      m[m[DIC]] |= RUN; /* set instruction to RUN */
			      m[DIC]++; /* compilation start here */ break;
		case READ: 
				if(forth_get_word(o, o->s) < 0)
					goto end;
				if ((w = forth_find(o, (char*)o->s)) > 1) {
					pc = w;
					if (!m[STATE] && instruction(m[ck(pc)]) == COMPILE)
						pc++; /* in command mode, execute word */
					goto INNER;
				} else if(!numberify(o->m[BASE], &w, (char*)o->s)) {
					fprintf(stderr, "( error \"%s is not a word\" )\n", o->s);
					break;
				}
				if (m[STATE]) { /* must be a number then */
					m[m[DIC]++] = 2; /*fake word push at m[2]*/
					m[ck(m[DIC]++)] = w;
				} else { /* push word */
					*++S = f;
					f = w;
				} break;
		case LOAD:    f = m[ck(f)];                          break;
		case STORE:   m[ck(f)] = *S--; f = *S--;             break;
		case SUB:     f = *S-- - f;                          break;
		case ADD:     f = *S-- + f;                          break;
		case AND:     f = *S-- & f;                          break;
		case OR:      f = *S-- | f;                          break;
		case XOR:     f = *S-- ^ f;                          break;
		case INV:     f = ~f;                                break;
		case SHL:     f = *S-- << f;                         break;
		case SHR:     f = (forth_cell_t)*S-- >> f;           break;
		case MUL:     f = *S-- * f;                          break;
		case DIV:     if(f) 
				      f = *S-- / f; 
			      else /* should throw exception */
				      fputs("( error \"x/0\" )\n", stderr); 
			                                             break;
		case LESS:    f = *S-- < f;                          break;
		case MORE:    f = *S-- > f;                          break;
		case EXIT:    I = m[ck(m[RSTK]--)];                  break;
		case EMIT:    fputc(f, (FILE*)(o->m[FOUT])); f = *S--; break;
		case KEY:     *++S = f; f = forth_get_char(o);       break;
		case FROMR:   *++S = f; f = m[ck(m[RSTK]--)];        break;
		case TOR:     m[ck(++m[RSTK])] = f; f = *S--;        break;
		case BRANCH:  I += m[ck(I)];                         break;
		case QBRANCH: I += f == 0 ? m[I] : 1; f = *S--;      break;
		case PNUM:    print_cell(o, f); f = *S--;            break;
		case QUOTE:   *++S = f;     f = m[ck(I++)];          break;
		case COMMA:   m[ck(m[DIC]++)] = f; f = *S--;         break;
		case EQUAL:   f = *S-- == f;                         break;
		case SWAP:    w = f;  f = *S--;   *++S = w;          break;
		case DUP:     *++S = f;                              break;
		case DROP:    f = *S--;                              break;
		case OVER:    w = *S; *++S = f; f = w;               break;
		case TAIL:    m[RSTK]--;                             break;
		case BSAVE:   f = blockio(o, m, *S--, f, 'w');       break;
		case BLOAD:   f = blockio(o, m, *S--, f, 'r');       break;
		case FIND:    *++S = f;
			      if(forth_get_word(o, o->s) < 0) 
				      goto end;
			      f = forth_find(o, (char*)o->s);
			      f = f < DICTIONARY_START ? 0 : f;      break;
		case PRINT:   fputs(((char*)m)+f, (FILE*)(o->m[FOUT])); f = *S--; break;
		case DEPTH:   w = S - (m + o->core_size - (2 * o->m[STACK_SIZE]));
			      *++S = f;
			      f = w;                                 break;
		case CLOCK:   *++S = f;
			      f = ((1000 * clock()) - o->m[START_TIME]) / CLOCKS_PER_SEC;
			                                             break;
		default:      
			fprintf(stderr, "( fatal 'illegal-op %" PRIuCell " )\n", w);
			longjmp(*o->on_error, 1);
		}
	}
end:	o->S = S;
	o->m[TOP] = f;
	return 0;
}

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
	fprintf(stderr, "usage: %s [-s file] [-e string] [-l file] [-t] [-h] [-m size] [-] files\n", name);
}

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
\t-         stop processing options\n\n\
Options must come before files to execute\n\n";
	fputs(help_text, stderr);
}

int main_forth(int argc, char **argv) 
{
	FILE *in = NULL, *dump = NULL;
	int save = 0, readterm = 0, mset = 0, eval = 0, rval = 0, i = 1, c = 0;
	static const size_t kbpc = 1024/sizeof(forth_cell_t); /*kilobytes per cell*/
	static const char *dump_name = "forth.core";
	forth_cell_t core_size = DEFAULT_CORE_SIZE;
	forth_t *o = NULL;
	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		switch(argv[i][1]) { /**@todo there should be an -e option to evaluate string */
		case '\0': goto done; /* stop argument processing */
		case 'h':  usage(argv[0]); help(); return -1;
		case 't':  readterm = 1; break;
		case 'e':  if(i >= (argc - 1))
				   goto fail;
			   if(!(o = o ? o : forth_init(core_size, stdin, stdout))) {
				   fprintf(stderr, "error: initialization failed\n");
				   return -1;
			   }
			   if(forth_eval(o, argv[++i]) < 0)
				   goto end;
			   eval = 1;
			   break;
		case 's':  if(i >= (argc - 1))
				   goto fail;
			   dump_name = argv[++i];
		case 'd':  /*use default name*/
			   save = 1; 
			   break;
		case 'm':  if(o || (i >= argc - 1) || !numberify(10, &core_size, argv[++i]))
				   goto fail;
			   if((core_size *= kbpc) < MINIMUM_CORE_SIZE) {
				   fprintf(stderr, "error: -m too small (minimum %zu)\n", MINIMUM_CORE_SIZE / kbpc);
				   return -1;
			   }
			   mset = 1;
			   break;
		case 'l':  if(o || mset || (i >= argc - 1))
				   goto fail;
			   if(!(o = forth_load_core(dump = fopen_or_die(argv[++i], "rb")))) {
				   fprintf(stderr, "error: %s: core load failed\n", argv[i]);
				   return -1;
			   }
			   fclose(dump);
			   break;
		default:
		fail:
			fprintf(stderr, "error: invalid arguments\n");
			usage(argv[0]);
			return -1;
		}
done:
	readterm = (!eval && i == argc) || readterm; /* if no files are given, read stdin */
	if(!o && !(o = forth_init(core_size, stdin, stdout))) {
		fprintf(stderr, "error: forth initialization failed\n");
		return -1;
	}
	forth_set_args(o, argc, argv);
	for(; i < argc; i++) { /* process all files on command line */
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
		forth_set_file_input(o, stdin);
		rval = forth_run(o);
	}
end:	
	fclose_input(&in);
	if(save) { /* save core file */
		if(rval || o->m[INVALID]) {
			fprintf(stderr, "error: refusing to save invalid core\n");
			return -1;
		}
		if(forth_save_core(o, dump = fopen_or_die(dump_name, "wb"))) {
			fprintf(stderr, "error: core file save to '%s' failed\n", dump_name);
			rval = -1;
		}
		fclose(dump); 
	}
	forth_free(o);
	return rval;
}

