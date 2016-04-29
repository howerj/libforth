/** @file       libforth.c
 *  @brief      A FORTH library, based on <www.ioccc.org/1992/buzzard.2.c>
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 * 
 *  @todo The string storage should be made clear, removing vestiges
 *  of the previous string storage mechanism
 *  @todo A new Forth word format needs making.
 *  @todo Port this to a micro controller, and a Linux kernel module device
 *  driver (just because).
 *  @todo Make a better interface to the library, allowing the stack to
 *  be accessed and different size interpreters to be made.
 *  @todo Routines for saving and loading the image should be made
 *  @todo The hide bit should probably be moved else where instead of being
 *  in the word pointer
 **/

#include "libforth.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define CORE_SIZE  ((UINT16_MAX + 1u) / 2u) /**< virtual machine memory size*/

static const char *core_file= "forth.core";
static const char *initial_forth_program = "\\ FORTH startup program.       \n\
: state 8 ! exit : ; immediate ' exit , 0 state exit : hex 9 ! ; : pwd 10 ; \n\
: h 0 ; : r 1 ; : here h @ ; : [ immediate 0 state ; : ] 1 state ;          \n\
: :noname immediate here 2 , ] ; : if immediate ' jz , here 0 , ;           \n\
: else immediate ' j , here 0 , swap dup here swap - swap ! ;               \n\
: then immediate dup here swap - swap ! ; : 2dup over over ;                \n\
: begin immediate here ; : until immediate ' jz , here - , ;                \n\
: 0= 0 = ; : 1+ 1 + ; : 1- 1 - ; : ')' 41 ; : tab 9 emit ; : cr 10 emit ;   \n\
: .( key drop begin key dup ')' = if drop exit then emit 0 until ;          \n\
: line dup . tab dup 4 + swap begin dup @ . tab 1+ 2dup = until drop ;      \n\
: literal 2 , , ; : size [ 11 @ literal ] ; : string-offset [ 12 @ literal ] ; \n\
: list swap begin line cr 2dup < until ; : allot here + h ! ;               \n\
: words pwd @ begin dup 1 + @ size string-offset * + print tab @ dup 32 < until cr ; \n\
: tuck swap over ; : nip swap drop ; : rot >r swap r> swap ;                \n\
: -rot rot rot ; : ? 0= if [ find \\ , ] then ; : :: [ find : , ] ;";

static const forth_cell_t 
        block_size = 1024u,  /**< size of a FORTH block, in bytes*/
	hide            = 1 << ((CHAR_BIT * sizeof(hide)) - 1), /**< hide bit*/
	string_offset   = 32u,   /**< offset for strings (@todo remove the need for this!)*/
	max_word_length = 32u;

struct forth {	        /**< The FORTH environment is contained in here*/
	jmp_buf error;
	FILE *in;       /**< file input, if not using string input*/
	FILE *out;      /**< file output */  
	uint8_t *s;     /**< string storage pointer */
	uint8_t *sin;   /**< string input pointer*/
	size_t core_size; /**< size of VM */
	size_t stack_size; /**< size of variable and return stacks */
	size_t slen;    /**< string input length*/
	size_t sidx;    /**< string input index*/
	unsigned stringin :1; /**< string used if true (*sin), *in otherwise */
	unsigned invalid  :1; /**< if true, this object is invalid*/
	forth_cell_t I;	 /**< instruction pointer*/
	forth_cell_t *S; /**< stack pointer*/
	forth_cell_t m[]; /**< Forth Virtual Machine memory */
};

enum registers    { DIC=0/*m[0]*/,RSTK=1,STATE=8,HEX=9,PWD=10,INFO=11 };
enum instructions { PUSH,COMPILE,RUN,DEFINE,IMMEDIATE,COMMENT,READ,LOAD,STORE,
SUB,ADD,AND,OR,XOR,INV,SHL,SHR,MUL,LESS,EXIT,EMIT,KEY,FROMR,TOR,JMP,JMPZ,PNUM,
QUOTE,COMMA,EQUAL,SWAP,DUP,DROP,OVER,TAIL,BSAVE,BLOAD,FIND,PRINT,PSTK,LAST };

static char *names[] = { "read","@","!","-","+","&","|","^","~","<<",">>","*",
"<", "exit","emit","key","r>",">r","j","jz",".","'",",","=", "swap","dup",
"drop", "over", "tail","save","load","find","print",".s", NULL }; 

static int forth_get_char(forth_t *o) /**< get a char from string-in or a file*/
{ 
	if(o->stringin) 
		return o->sidx >= o->slen ? EOF : o->sin[o->sidx++];
	else 
		return fgetc(o->in);
} 

static FILE *fopen_or_die(const char *name, char *mode) 
{
	FILE *file = fopen(name, mode);
	if(!file) {
		fprintf(stderr, "( fatal 'file-open \"%s: %s\" )\n", name, errno ? strerror(errno): "unknown");
		exit(EXIT_FAILURE);
	}
	return file;
}

/*get a word (space delimited, up to 31 chars) from a FILE* or string-in*/
static int forth_get_word(forth_t *o, uint8_t *p) 
{ 
	int n = 0;
	char fmt[16] = { 0 };
	sprintf(fmt, "%%%ds%%n", max_word_length - 1);
	if(o->stringin) {
		if(sscanf((char *)&(o->sin[o->sidx]), fmt, p, &n) < 0)
			return EOF;
		return o->sidx += n, n;
	} else  {
		return fscanf(o->in, fmt, p, &n);
	}
} 

static int compile(forth_t *o, forth_cell_t code, char *str) 
{ 
	assert(o && code < LAST);
	forth_cell_t *m = o->m, header = m[DIC];
	size_t l = 0;
	if(!str) {
		if(forth_get_word(o, (uint8_t*)(o->s)) < 0)
			return -1;
		str = (char *)o->s;
	} 
	/*FORTH header structure*/
	strcpy((char *)(o->m + header), str); /* 0: Copy the new FORTH word into the new header */
	l = strlen(str) + 1;
	l = (l + (sizeof(forth_cell_t) - 1)) & ~(sizeof(forth_cell_t) - 1); /* align up to sizeof word */
	m[DIC] += (l/sizeof(forth_cell_t)); /* Add string length in words to header (STRLEN) */

	m[m[0]++] = m[PWD];     /*0 + STRLEN: Pointer to previous words header*/
	m[PWD] = m[0] - 1;      /*   Update the PWD register to new word */
	m[m[0]++] = (header*sizeof(forth_cell_t)) - (string_offset*sizeof(forth_cell_t));  /*1 + STRLEN: Point to string*/
	m[m[0]++] = code;       /*2 + STRLEN: Add in VM code to run for this word*/
	return 0;
}

static int blockio(forth_t *o, void *p, forth_cell_t poffset, forth_cell_t id, char rw) 
{ 
	char name[16] = {0}; /* XXXX + ".blk" + '\0' + a little spare change */
	FILE *file = NULL;
	size_t n;
	if(poffset > (o->core_size - block_size))
		return -1;
	sprintf(name, "%04x.blk", (int)id);
	if(!(file = fopen(name, rw == 'r' ? "rb" : "wb"))) {
		fprintf(stderr, "( error 'file-open \"%s : could not open file\" )\n", name);
		return -1;
	}
	n = rw == 'w' ? fwrite(((char*)p) + poffset, 1, block_size, file):
			fread (((char*)p) + poffset, 1, block_size, file);
	fclose(file);
	return n == block_size ? 0 : -1;
} /*a basic FORTH block I/O interface*/

static int isnum(const char *s)  /**< is word a number? */
{
	s += *s == '-' ? 1 : 0; 
	return s[strspn(s, "0123456789")] == '\0'; 
}

static int comment(forth_t *o)  /**< process a comment line*/
{
	int c; 
	while(((c = forth_get_char(o)) > 0) && (c != '\n')); 
	return c;
} 

static forth_cell_t find(forth_t *o) 
{ 
	forth_cell_t *m = o->m, w = m[PWD];
	for (;(w & hide) || strcmp((char*)o->s,(char*)&o->s[m[(w & ~hide)+1]]);)
		w = m[w & ~hide]; /*top bit, or hide bit, hides the word*/
	return w;
} /*find a word in the FORTH dictionary*/

static int print_stack(forth_t *o, forth_cell_t f, forth_cell_t *S)
{ 
	forth_cell_t *begin = o->m + o->core_size - (2*o->stack_size);
	if(fprintf(o->out, o->m[HEX] ? "%hx\t" : "%hu\t", f) < 0) return -1;
	while(begin + 1 < S)
		if(fprintf(o->out, o->m[HEX] ? "%hx\t" : "%hu\t", *(S--)) < 0) 
			return -1;
	return 0;
} /*print the forth stack*/

void forth_set_file_input(forth_t *o, FILE *in) 
{ 
	assert(o && in); 
	o->stringin = 0; 
	o->in = in; 
}

void forth_set_file_output(forth_t *o, FILE *out) 
{ 
	assert(o && out); 
	o->stringin = 0; 
	o->out = out; 
}

void forth_set_string_input(forth_t *o, const char *s) 
{ 
	assert(o && s);
	o->sidx = 0;	     /*sidx == current character in string*/
	o->slen = strlen(s) + 1; /*slen == actual string len*/
	o->stringin = 1;	 /*read from string not a file handle*/
	o->sin = (uint8_t *)s;   /*sin  == pointer actual string*/
}

int forth_eval(forth_t *o, const char *s) 
{ 
	assert(o && s); 
	forth_set_string_input(o,s); 
	return forth_run(o);
}

int forth_dump_core(forth_t *o, FILE *dump) 
{ 
	assert(o && dump);
	size_t w = sizeof(*o) + sizeof(forth_cell_t) * o->core_size;
	return w != fwrite(o, 1, w, dump) ? -1: 0; 
}

forth_t *forth_init(size_t size, FILE *in, FILE *out) 
{ 
	assert(in && out);
	forth_cell_t *m, i, w;
	forth_t *o;
	if(size < MINIMUM_CORE_SIZE)
		return NULL;

	if(!(o = calloc(1, sizeof(*o) + sizeof(forth_cell_t)*size))) 
		return NULL;
	o->core_size = size;
	o->stack_size = size / 64;
	m = o->m;       /*a local variable only for convenience*/
	o->s = (uint8_t*)(m + string_offset); /*string store offset into CORE, skip register*/
	o->out = out;   /*this will be used for standard output*/

	w = m[DIC] = string_offset + max_word_length; /*initial dictionary offset, skip registers and string offset*/
	m[PWD]     = 1;     /*special terminating pwd*/
	m[INFO]    = sizeof(forth_cell_t);
	m[INFO+1]  = string_offset;
	m[INFO+2]  = size;

	m[m[DIC]++] = READ; /*create a special word that reads in FORTH*/
	m[m[DIC]++] = RUN;  /*call the special word recursively*/
	o->I = m[DIC];      /*instruction stream points to our special word*/
	m[m[DIC]++] = w;    /*recursive call to that word*/
	m[m[DIC]++] = o->I - 1; /*execute read*/

	compile(o, DEFINE,    ":");	    /*immediate word*/
	compile(o, IMMEDIATE, "immediate"); /*immediate word*/
	compile(o, COMMENT,   "\\");	    /*immediate word*/
	for(i = 0, w = READ; names[i]; i++) /*compiling words*/
		compile(o, COMPILE, names[i]), m[m[DIC]++] = w++;
	m[RSTK] = size - o->stack_size;	   /*set up return stk pointer*/
	o->S    = m + size - (2 * o->stack_size); /*set up variable stk pointer*/
	if(forth_eval(o, initial_forth_program) < 0) 
		return NULL; /*define words*/
	forth_set_file_input(o, in);	/*set up input after out eval*/
	return o;
}

void forth_free(forth_t *o) 
{ 
	assert(o); 
	free(o); 
}

static forth_cell_t check_bounds(forth_t *o, forth_cell_t f) 
{ 
	if(f >= o->core_size) {
		fprintf(stderr, "( fatal \"bounds check failed: %d >= %zu\" )\n", f, o->core_size);
		longjmp(o->error, 1);
	}
	return f; 
}

#define ck(c) check_bounds(o, c)

void forth_push(forth_t *o, forth_cell_t f)
{
	assert(o && o->S < o->m + o->core_size);
	*++(o->S) = f;
}

forth_cell_t forth_pop(forth_t *o)
{
	assert(o && o->S > o->m);
	return *(o->S)--;
}

forth_cell_t forth_stack_position(forth_t *o)
{
	return o->S - o->m;
}

int forth_run(forth_t *o) 
{ 
	assert(o);
	forth_cell_t *m, pc, *S, I, f = 0, w;
	m = o->m, S = o->S, I = o->I; 

	if(o->invalid || setjmp(o->error))
		return -(o->invalid = 1);

	for(;(pc = m[ck(I++)]);) { /* Threaded code interpreter */
		assert((S > m) && (S < (m + o->core_size)));
	INNER:  
		switch (m[ck(pc++)]) {
		case PUSH:    *++S = f;     f = m[ck(I++)];        break;
		case COMPILE: m[ck(m[DIC]++)] = pc;                break;
		case RUN:     m[ck(++m[RSTK])] = I; I = pc;        break;
		case DEFINE:  m[STATE] = 1;
			      if(compile(o, COMPILE, NULL) < 0)
				      return 0;
			      m[ck(m[DIC]++)] = RUN;               break;
		case IMMEDIATE: *m -= 2; m[m[DIC]++] = RUN;        break;
		case COMMENT: if(comment(o) < 0) return 0;         break;
		case READ:    
			m[ck(RSTK)]--;
			if(forth_get_word(o, o->s) < 1)
				return 0;
			if ((w = find(o)) > 1) {
				pc = w + 2;
				if (!m[STATE] && m[ck(pc)] == COMPILE)
					pc++;
				goto INNER;
			} else if(!isnum((char*)o->s)) {
				fprintf(stderr, "( error \"%s is not a word\" )\n", o->s);
				break;
			}
			if (m[STATE]) { /*must be a number then*/
				m[m[0]++] = 2; /*word push at m[2]*/
				m[ck(m[0]++)] = strtol((char*)o->s,0,0);
			} else {
				*++S = f;
				f = strtol((char*)o->s, 0, 0);
			}                                            break;
		case LOAD:    f = m[ck(f)];                          break;
		case STORE:   m[ck(f)] = *S--; f = *S--;             break;
		case SUB:     f = *S-- - f;                          break;
		case ADD:     f = *S-- + f;                          break;
		case AND:     f = *S-- & f;                          break;
		case OR:      f = *S-- | f;                          break;
		case XOR:     f = *S-- ^ f;                          break;
		case INV:     f = ~f;                                break;
		case SHL:     f = *S-- << f;                         break;
		case SHR:     f = *S-- >> f;                         break;
		case MUL:     f = *S-- * f;                          break;
		case LESS:    f = *S-- < f;                          break;
		case EXIT:    I = m[ck(m[RSTK]--)];                  break;
		case EMIT:    fputc(f, o->out); f = *S--;            break;
		case KEY:     *++S = f; f = forth_get_char(o);       break;
		case FROMR:   *++S = f; f = m[ck(m[RSTK]--)];        break;
		case TOR:     m[ck(++m[RSTK])] = f; f = *S--;        break;
		case JMP:     I += m[ck(I)];                         break;
		case JMPZ:    I += f == 0 ? m[I] : 1; f = *S--;      break;
		case PNUM:    fprintf(o->out, m[HEX] ? "%X" : "%u", f);
			      f = *S--;                              break;
		case QUOTE:   *++S = f;     f = m[ck(I++)];          break;
		case COMMA:   m[ck(m[0]++)] = f; f = *S--;           break;
		case EQUAL:   f = *S-- == f;                         break;
		case SWAP:    w = f;  f = *S--;   *++S = w;          break;
		case DUP:     *++S = f;                              break;
		case DROP:    f = *S--;                              break;
		case OVER:    w = *S; *++S = f; f = w;               break;
		case TAIL:    m[RSTK]--;                             break;
		case BSAVE:   f = blockio(o, m, *S--, f, 'w');       break;
		case BLOAD:   f = blockio(o, m, *S--, f, 'r');       break;
		case FIND:    *++S = f;
			      if(forth_get_word(o, o->s) < 1) 
				      return 0 /*EOF*/;
			      f = find(o) + 2;
			      f = f < max_word_length ? 0 : f;        break;
		case PRINT:   fputs(((char*)m)+f, o->out); f = *S--;  break;
		case PSTK:    if(print_stack(o, f ,S) < 0) return -1; break;
		default:      
			fputs("( fatal 'illegal-op )\n", stderr);
			abort();
		}
	}
	return 0;
}

int main_forth(int argc, char **argv) 
{ 	/*options: ./forth (-d)? (file)* */
	int dump = 0, rval = 0, c;      /*dump on?, return value, temp char*/
	FILE *in = NULL, *coreo = NULL; /*current input file, dump file*/
	forth_t *o = NULL;		/*our FORTH environment*/
	assert(argv);
	if(!(o = forth_init(CORE_SIZE, stdin, stdout))) 
		return -1; /*setup env*/
	if(argc > 1 && !strcmp(argv[1], "-d")) /*turn core dump on*/
			argv++, argc--, dump = 1;
	if(argc > 1) {
		while(++argv, --argc) {
			forth_set_file_input(o, in = fopen_or_die(argv[0], "rb"));
/*shebang line '#!' */  if((c = fgetc(in)) == '#') {
				comment(o); /*special case*/
			} else if (c == EOF) { 
				fclose(in), in = NULL; 
				continue; 
			} else {
				ungetc(c,in);
			}
			if((rval = forth_run(o)) < 0) 
				goto END;
			fclose(in), in = NULL;
		}
	} else { 
		rval = forth_run(o); /*read from defaults, stdin*/
	}
END:	
	if(in && (in != stdin)) 
		fclose(in), in = NULL;
	if(dump) { 
		if(forth_dump_core(o, coreo = fopen_or_die(core_file, "wb")) < 0)
			return -1;
		fclose(coreo); 
	}
	forth_free(o), o = NULL;
	return rval;
}

