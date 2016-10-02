/** @file       forth.c
 *  @brief      A version of the libforth interpreter that is does
 *              no memory allocations, which can be used as a starting
 *              point to porting to more esoteric platforms.
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2016 Richard James Howe.
 *  @license    MIT (see https://opensource.org/licenses/MIT)
 *  @email      howe.r.j.89@gmail.com */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uintptr_t forth_cell_t; 

#define PRIuCell PRIdPTR
#define PRIxCell PRIxPTR
#define CORE_SIZE           (2048) /**< default vm size*/
#define STRING_OFFSET       (32u)   /**< offset into memory of string buffer*/
#define MAX_WORD_LENGTH     (32u)   /**< max word length, must be < 255 */
#define DICTIONARY_START    (STRING_OFFSET + MAX_WORD_LENGTH) /**< start of dic */
#define WORD_LENGTH_OFFSET  (8)  /**< bit offset for word length start*/
#define WORD_LENGTH(FIELD1) (((FIELD1) >> WORD_LENGTH_OFFSET) & 0xff)
#define WORD_HIDDEN(FIELD1) ((FIELD1) & 0x80) /**< is a forth word hidden? */
#define INSTRUCTION_MASK    (0x7f)
#define instruction(k)      ((k) & INSTRUCTION_MASK)
#define VERIFY(X)           do { if(!(X)) { abort(); } } while(0)

static const char *initial_forth_program = "\n\
: state 8 exit : ; immediate ' exit , 0 state ! exit : base 9 ; : pwd 10 ;   \n\
: h 6 ; : r 7 ; : here h @ ; : [ immediate 0 state ! ; : ] 1 state ! ;       \n\
: :noname immediate here 2 , ] ; : if immediate ' ?branch , here 0 , ;       \n\
: else immediate ' branch , here 0 , swap dup here swap - swap ! ; : 0= 0 = ;\n\
: then immediate dup here swap - swap ! ; : 2dup over over ; : <> = 0= ;     \n\
: begin immediate here ; : until immediate ' ?branch , here - , ; : '\\n' 10 ; \n\
: not 0= ; : 1+ 1 + ; : 1- 1 - ; : ')' 41 ; : tab 9 emit ; : cr '\\n' emit ; \n\
: ( immediate begin key ')' = until ; : rot >r swap r> swap ; : -rot rot rot ;\n\
: tuck swap over ; : nip swap drop ; : :: [ find : , ] ; : allot here + h ! ; ";

static uint8_t *s;          /**< convenience pointer for string input buffer */
static char hex_fmt[16];    /**< calculated hex format*/
static forth_cell_t I;      /**< instruction pointer */
static forth_cell_t f;      /**< top of stack */
static forth_cell_t pc;     /**< vm program counter */
static forth_cell_t w;      /**< working pointer */
static forth_cell_t *S;     /**< stack pointer */
static forth_cell_t m[CORE_SIZE]; /**< Forth Virtual Machine memory !! (as is)*/
static forth_cell_t compile_header; /**< used by compile */
static forth_cell_t compile_len;    /**< used by compile */
static char tmp_constant[MAX_WORD_LENGTH+32];

static void forth_set_file_input(FILE *in);
static void forth_set_string_input(const char *s);
static int  forth_eval(const char *s);

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
enum instructions { PUSH,COMPILE,RUN,DEFINE,IMMEDIATE,READ,LOAD,STORE,
SUB,ADD,AND,OR,XOR,INV,SHL,SHR,MUL,DIV,LESS,MORE,EXIT,EMIT,KEY,FROMR,TOR,BRANCH,
QBRANCH, PNUM, QUOTE,COMMA,EQUAL,SWAP,DUP,DROP,OVER,FIND,PRINT,
DEPTH,LAST };

static const char *names[] = { "read","@","!","-","+","and","or","xor","invert",
"lshift","rshift","*","/","u<","u>","exit","emit","key","r>",">r","branch",
"?branch", ".","'", ",","=", "swap","dup","drop", "over", 
"find", "print","depth", NULL }; 

static int forth_get_char() 
{ /* get a char from string input or a file */
	switch(m[SOURCE_ID]) {
	case FILE_IN:   return fgetc((FILE*)(m[FIN]));
	case STRING_IN: return m[SIDX] >= m[SLEN] ? EOF : ((char*)(m[SIN]))[m[SIDX]++];
	default:        return EOF;
	}
} 

static int readin;
static int forth_get_word(uint8_t *p) 
{ /*get a word (space delimited, up to 31 chars) from a FILE* or string-in*/ 
	readin = 0;
	switch(m[SOURCE_ID]) {
	case FILE_IN:   return fscanf((FILE*)(m[FIN]), "%31s%n", p, &readin);
	case STRING_IN:
		if(sscanf((char *)&(((char*)(m[SIN]))[m[SIDX]]), "%31s%n", p, &readin) < 0)
			return EOF;
		return m[SIDX] += readin, readin;
	default:       return EOF;
	}
} 

static void compile(forth_cell_t code, const char *str) 
{ /* create a new forth word header */
	compile_header = m[DIC];
	compile_len = 0;
	/*FORTH header structure*/
	strcpy((char *)(m + compile_header), str); /* 0: Copy the new FORTH word into the new header */
	compile_len = strlen(str) + 1;
	compile_len = (compile_len + (sizeof(forth_cell_t) - 1)) & ~(sizeof(forth_cell_t) - 1); /* align up to sizeof word */
	compile_len = compile_len/sizeof(forth_cell_t);
	m[DIC] += compile_len; /* Add string length in words to header (STRLEN) */

	m[m[DIC]++] = m[PWD];     /*0 + STRLEN: Pointer to previous words header*/
	m[PWD] = m[DIC] - 1;      /*   Update the PWD register to new word */
	m[m[DIC]++] = (compile_len << WORD_LENGTH_OFFSET) | code; /*1: size of words name and code field */
}

static int numberify(int base, forth_cell_t *n, const char *s)  
{ /*returns non zero if conversion was successful*/
	char *end = NULL;
	errno = 0;
	*n = strtol(s, &end, base);
	return !errno && *s != '\0' && *end == '\0';
}

static forth_cell_t forth_find(const char *s) 
{ /* find a word in the Forth dictionary, which is a linked list, skipping hidden words */
	forth_cell_t x = m[PWD];
	forth_cell_t len = WORD_LENGTH(m[x+1]);
	for (;x > DICTIONARY_START && (WORD_HIDDEN(m[x+1]) || strcmp(s,(char*)(&m[x - len])));) {
		x = m[x]; 
		len = WORD_LENGTH(m[x+1]);
	}
	return x > DICTIONARY_START ? x+1 : 0;
}

static int print_cell(forth_cell_t f)
{ 
	char *fmt = m[BASE] == 16 ? hex_fmt : "%" PRIuCell;
	return fprintf((FILE*)(m[FOUT]), fmt, f);
}

static int forth_define_constant(const char *name, forth_cell_t c)
{
	memset(tmp_constant, 0, MAX_WORD_LENGTH+32);
	sprintf(tmp_constant, ": %31s %" PRIuCell " ; \n", name, c);
	return forth_eval(tmp_constant);
}

static void forth_make_default(FILE *in, FILE *out)
{ /* set defaults for a forth structure for initialization or reload */
	m[STACK_SIZE] = CORE_SIZE / 64;
	s             = (uint8_t*)(m + STRING_OFFSET); /*string store offset into CORE, skip registers*/
	m[FOUT]       = (forth_cell_t)out;
	m[START_ADDR] = (forth_cell_t)&(m);
	m[STDIN]      = (forth_cell_t)stdin;
	m[STDOUT]     = (forth_cell_t)stdout;
	m[STDERR]     = (forth_cell_t)stderr;
	m[RSTK]       = CORE_SIZE - m[STACK_SIZE];     /*set up return stk pointer*/
	m[ARGC] = m[ARGV] = 0;
	S             = m + CORE_SIZE - (2 * m[STACK_SIZE]); /*set up variable stk pointer*/
	sprintf(hex_fmt, "0x%%0%d"PRIxCell, (int)(sizeof(forth_cell_t)*2));
	forth_set_file_input(in);  /*set up input after our eval*/
}

int forth_init(FILE *in, FILE *out) 
{ 
	forth_cell_t i, p;
	forth_make_default(in, out);

	m[PWD]   = 0;  /*special terminating pwd value*/
	p = m[DIC]  = DICTIONARY_START; /*initial dictionary offset, skip registers and string offset*/
	m[m[DIC]++] = READ; /*create a special word that reads in FORTH*/
	m[m[DIC]++] = RUN;  /*call the special word recursively*/
	m[INSTRUCTION] = m[DIC]; /*instruction stream points to our special word*/
	m[m[DIC]++] = p;    /*recursive call to that word*/
	m[m[DIC]++] = m[INSTRUCTION] - 1; /*execute read*/

	compile(DEFINE,    ":");         /*immediate word*/
	compile(IMMEDIATE, "immediate"); /*immediate word*/
	for(i = 0, p = READ; names[i]; i++) /*compiling words*/
		compile(COMPILE, names[i]), m[m[DIC]++] = p++;

	VERIFY(forth_eval(initial_forth_program) >= 0);
	VERIFY(forth_define_constant("size",          sizeof(forth_cell_t)) >= 0);
	VERIFY(forth_define_constant("stack-start",   CORE_SIZE - (2 * m[STACK_SIZE])) >= 0);
	VERIFY(forth_define_constant("max-core",      CORE_SIZE) >= 0);
	VERIFY(forth_define_constant("source-id-reg", SOURCE_ID) >= 0);

	forth_set_file_input(in);  /*set up input after our eval*/
	return 0;
}

static int forth_run(void) 
{ /* this implements the Forth virtual machine; it does all the work */ 
	f = m[TOP];
	I = m[INSTRUCTION];

	for(;(pc = m[I++]);) { /* Threaded code interpreter */
	INNER:  
		switch (w = instruction(m[pc++])) {
		case PUSH:    *++S = f;     f = m[I++];          break;
		case COMPILE: m[m[DIC]++] = pc;                  break;
		case RUN:     m[++m[RSTK]] = I; I = pc;          break;
		case DEFINE:  m[STATE] = 1; /* compile mode */
                              if(forth_get_word((uint8_t*)(s)) < 0)
                                      goto end;
                              compile(COMPILE, (char*)s); 
                              m[m[DIC]++] = RUN;                 break;
		case IMMEDIATE: 
			      m[DIC] -= 2; /* move to first code field */
			      m[m[DIC]] &= ~INSTRUCTION_MASK; /* zero instruction */
			      m[m[DIC]] |= RUN; /* set instruction to RUN */
			      m[DIC]++; /* compilation start here */ break;
		case READ: 
			m[RSTK]--; /* bit of a hack */
			if(forth_get_word(s) < 0)
				goto end;
			if ((w = forth_find((char*)s)) > 1) {
				pc = w;
				if (!m[STATE] && instruction(m[pc]) == COMPILE)
					pc++; /* in command mode, execute word */
				goto INNER;
			} else if(!numberify(m[BASE], &w, (char*)s)) {
				fprintf(stderr, "( error \"%s is not a word\" )\n", s);
				break;
			}
			if (m[STATE]) { /* must be a number then */
				m[m[DIC]++] = 2; /*fake word push at m[2]*/
				m[m[DIC]++] = w;
			} else { /* push word */
				*++S = f;
				f = w;
			}                                            break;
		case LOAD:    f = m[f];                              break;
		case STORE:   m[f] = *S--; f = *S--;                 break;
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
		case EXIT:    I = m[m[RSTK]--];                      break;
		case EMIT:    fputc(f, (FILE*)(m[FOUT])); f = *S--;  break;
		case KEY:     *++S = f; f = forth_get_char();        break;
		case FROMR:   *++S = f; f = m[m[RSTK]--];            break;
		case TOR:     m[++m[RSTK]] = f; f = *S--;            break;
		case BRANCH:  I += m[I];                             break;
		case QBRANCH: I += f == 0 ? m[I] : 1; f = *S--;      break;
		case PNUM:    print_cell(f); f = *S--;               break;
		case QUOTE:   *++S = f;     f = m[I++];              break;
		case COMMA:   m[m[DIC]++] = f; f = *S--;             break;
		case EQUAL:   f = *S-- == f;                         break;
		case SWAP:    w = f;  f = *S--;   *++S = w;          break;
		case DUP:     *++S = f;                              break;
		case DROP:    f = *S--;                              break;
		case OVER:    w = *S; *++S = f; f = w;               break;
		case FIND:    *++S = f;
			      if(forth_get_word(s) < 0) 
				      goto end;
			      f = forth_find((char*)s);
			      f = f < DICTIONARY_START ? 0 : f;      break;
		case PRINT:   fputs(((char*)m)+f, (FILE*)(m[FOUT])); f = *S--; break;
		case DEPTH:   w = S - (m + CORE_SIZE - (2 * m[STACK_SIZE]));
			      *++S = f;
			      f = w;                                 break;
		default:      
			fprintf(stderr, "( fatal 'illegal-op %" PRIuCell " )\n", w);
			abort();
		}
	}
end:
	m[TOP] = f;
	return 0;
}

static int forth_eval(const char *s) 
{ 
	forth_set_string_input(s); 
	return forth_run();
}

static void forth_set_file_input(FILE *in) 
{
	m[SOURCE_ID] = FILE_IN;
	m[FIN]       = (forth_cell_t)in; 
}

static void forth_set_string_input(const char *s) 
{ 
	m[SIDX] = 0;              /*m[SIDX] == current character in string*/
	m[SLEN] = strlen(s) + 1;  /*m[SLEN] == string len*/
	m[SOURCE_ID] = STRING_IN; /*read from string, not a file handle*/
	m[SIN] = (forth_cell_t)s; /*sin  == pointer to string input*/
}

int main(void)
{
	if(forth_init(stdin, stdout)) {
		fputs("could not initialize core\n",stderr);
		return -1;
	}
	return forth_run();
}
