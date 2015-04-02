/** @file       libforth.c
 *  @brief      A FORTH library, based on <www.ioccc.org/1992/buzzard.2.c>
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com **/
#include "libforth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <assert.h>

static const char *coref="forth.core", *initprg="# FORTH statup program.    \n\
: state 8 ! exit : ; immediate ' exit , 0 state exit : hex 9 ! ; : pwd 10 ; \n\
: h 0 ; : r 1 ; : here h @ ; : [ immediate 0 state ; : ] 1 state ;          \n\
: :noname immediate here 2 , ] ; : if immediate ' jz , here 0 , ;           \n\
: else immediate ' j , here 0 , swap dup here swap - swap ! ;               \n\
: then immediate dup here swap - swap ! ; : 2dup over over ;                \n\
: begin immediate here ; : until immediate ' jz , here - , ;                \n\
: 0= 0 = ; : 1+ 1 + ; : 1- 1 - ; : ')' 41 ; : tab 9 emit ; : cr 10 emit ;   \n\
: .( key drop begin key dup ')' = if drop exit then emit 0 until ;          \n\
: line dup . tab dup 4 + swap begin dup @ . tab 1+ 2dup = until drop ;      \n\
: list swap begin line cr 2dup < until ; : allot here + h ! ;               \n\
: words pwd @ begin dup 1 + @ 32768 + print tab @ dup 32 < until ;          \n\
: tuck swap over ; : nip swap drop ; : rot >r swap r> swap ;                \n\
: -rot rot rot ; : ? 0= if [ find # , ] then ;                              \n\
: :: [ find : , ] ; : create :: 2 , here 2 + , ' exit , 0 state ; ";
static const char *errorfmt = "( error \"%s%s\" %s %u )\n";

#define PWARN(P,M) fprintf(stderr, errorfmt, (P), (M), __FILE__, __LINE__)
#define WARN(M)    PWARN("",(M))
#define CORESZ     ((UINT16_MAX + 1u) / 2u) /*virtual machine memory size*/
#define STROFF     (CORESZ/2u)  /*string storage offset into memory*/
#define STKSZ      (CORESZ/64u) /*size of the variable and return stack*/
#define BLKSZ      (1024u)      /*size of a FORTH block, in bytes*/
#define CK(X)      ck(o, (X))   /*unsafe definition is (X), should be faster*/
#define HIDE       (0x8000u)    /*hide word if top bit is set of pwd ptr*/

struct forth_obj {         /*The FORTH environment is contained in here*/
        uint16_t pad1[8], m[CORESZ] /*FORTH VM Mem*/, pad2[8], I, *S;
        FILE *in, *out;    /*file input (if not using string input), output*/
        uint8_t *s, *sin;  /*string storage pointer, string input pointer*/
        size_t slen, sidx; /*string input length and index into buffer*/
        unsigned invalid :1, stringin :1; /*invalidate obj? string input? */
        jmp_buf exception; /*jump here on exception*/
};

static uint16_t ck(forth_obj_t *o, uint16_t f)
{ return f & 0x8000 ? WARN("bounds fail"), longjmp(o->exception, 1), 0 : f; }

enum registers    { DIC=0/*m[0]*/,RSTK=1,STATE=8,HEX=9,PWD=10,SSTORE=11 };
enum instructions { PUSH,COMPILE,RUN,DEFINE,IMMEDIATE,COMMENT,READ,LOAD,STORE,
SUB,ADD,AND,OR,XOR,INV,SHL,SHR,LESS,EXIT,EMIT,KEY,FROMR,TOR,JMP,JMPZ,PNUM,
QUOTE,COMMA,EQUAL,SWAP,DUP,DROP,OVER,TAIL,BSAVE,BLOAD,FIND,PRINT,PSTK,LAST };

static char *names[] = { "read","@","!","-","+","&","|","^","~","<<",">>","<",
"exit","emit","key","r>",">r","j","jz",".","'",",","=", "swap","dup","drop",
"over", "tail","save","load","find","print",".s", NULL }; 

static int ogetc(forth_obj_t *o)
{       if(o->stringin) return o->sidx >= o->slen ? EOF : o->sin[o->sidx++];
        else return fgetc(o->in);
} /*get a char from string-in or a file*/

static FILE *fopenj(const char *name, char *mode, jmp_buf *go)
{       FILE *file = fopen(name, mode);
        if(!file) perror(name), longjmp(*go, 1);
        return file;
} /*fopen with exception handling and printing*/

static int ogetwrd(forth_obj_t *o, uint8_t *p)
{       int n = 0;
        if(o->stringin){
                if(sscanf((char *)&(o->sin[o->sidx]), "%31s%n", p, &n) < 0)
                        return EOF;
                return o->sidx += n, n;
        } else  return fscanf(o->in, "%31s", p);
} /*get a word (space delimited, up to 31 chars) from a FILE* or string-in*/

static int compile(forth_obj_t *o, uint16_t code, char *str)
{       uint16_t *m = o->m;
        int r = 0;              /*FORTH header structure*/
        m[m[0]++] = m[PWD];     /*0: Pointer to previous words header*/
        m[PWD] = m[0] - 1;      /*   Update the PWD register to new word */
        m[m[0]++] = m[SSTORE];  /*1: Point to where new word is string is */
        m[m[0]++] = code;       /*2: Add in VM code to run for this word*/
        if (str) strcpy((char *)o->s + m[SSTORE], str);
        else     r = ogetwrd(o, o->s + m[SSTORE]);
        m[SSTORE] += strlen((char*)o->s + m[SSTORE]) + 1;
        return r;
} /*compile a word into the FORTH dictionary*/

static int blockio(void *p, uint16_t poffset, uint16_t id, char rw)
{       char name[16]; /* XXXX + ".blk" + '\0' + a little spare change */
        FILE *file;
        size_t n;
        if((poffset > (CORESZ - BLKSZ)) || !(rw == 'w' || rw == 'r'))
                return WARN("invalid address or mode"), -1;
        sprintf(name, "%04x.blk", (int)id);
        if(!(file = fopen(name, rw == 'r' ? "rb" : "wb")))
                return PWARN(name, ": could not open file"), -1;
        n = rw == 'w' ? fwrite(p+poffset, 1, BLKSZ, file):
                        fread (p+poffset, 1, BLKSZ, file);
        fclose(file);
        return n == BLKSZ ? 0 : -1;
} /*a basic FORTH block I/O interface*/

static int isnum(const char *s) /*needs fixing; proper octal and hex*/
{ s += *s == '-' ? 1 : 0; return s[strspn(s,"0123456789")] == '\0'; }

static uint16_t find(forth_obj_t *o)
{       uint16_t *m = o->m, w = m[PWD];
        for (;(w & HIDE)||strcmp((char*)o->s,(char*)&o->s[m[(w & ~HIDE)+1]]);)
                w = m[w & ~HIDE]; /*top bit, or HIDE bit, hides the word*/
        return w;
} /*find a word in the FORTH dictionary*/

static int comment(forth_obj_t *o) /*process a comment line from input*/
{ int c; while(((c = ogetc(o)) > 0) && (c != '\n')); return c; } 

static int pstk(forth_obj_t *o, uint16_t f, uint16_t *S)
{       uint16_t *begin = o->m + CORESZ - (2*STKSZ);
        if(fprintf(o->out,o->m[HEX]?"%hx\t":"%hu\t",f) < 0) return -1;
        while(begin+1<S)
                if(fprintf(o->out,o->m[HEX]?"%hx\t":"%hu\t",*(S--)) < 0) 
                        return -1;
        return 0;
} /*print the forth stack*/

void forth_seti(forth_obj_t *o, FILE *in)  
{ assert(o && in); o->stringin = 0; o->in = in;  }

void forth_seto(forth_obj_t *o, FILE *out) 
{ assert(o && out); o->stringin = 0; o->out = out; }

void forth_sets(forth_obj_t *o, const char *s)
{       assert(o && s);
        o->sidx = 0;             /*sidx == current character in string*/
        o->slen = strlen(s) + 1; /*slen == actual string len*/
        o->stringin = 1;         /*read from string not a file handle*/
        o->sin = (uint8_t *)s;   /*sin  == pointer actual string*/
}

int forth_eval(forth_obj_t *o, const char *s)
{ assert(o && s); forth_sets(o,s); return forth_run(o);}

int forth_coredump(forth_obj_t *o, FILE *dump)
{ return !o || !dump || sizeof(*o) != fwrite(o, 1, sizeof(*o), dump) ? -1: 0; }

forth_obj_t *forth_init(FILE *in, FILE *out)
{       uint16_t *m, i, w;
        forth_obj_t *o;

        if(!in || !out || !(o = calloc(1, sizeof(*o)))) return NULL;
        m = o->m;
        o->s = (uint8_t*)(m + STROFF); /*string store offset into CORE*/
        o->out = out;

        w = m[0] = 32;  /*initial dictionary offset, skip registers*/
        m[PWD] = 1;     /*special terminating pwd*/
        m[SSTORE] = 32; /*offset into str storage defines maximum word length*/

        m[m[0]++] = READ; /*create a special word that reads in FORTH*/
        m[m[0]++] = RUN;  /*call the special word recursively*/
        o->I = m[0];      /*instruction stream points to our special word*/
        m[m[0]++] = w;    /*recursive call to that word*/
        m[m[0]++] = o->I - 1; /*execute read*/

        compile(o, DEFINE,    ":");         /*immediate word*/
        compile(o, IMMEDIATE, "immediate"); /*immediate word*/
        compile(o, COMMENT,   "#");         /*immediate word*/
        for(i = 0, w = READ; names[i]; i++) /*compiling words*/
                compile(o, COMPILE, names[i]), m[m[0]++] = w++;
        m[RSTK] = CORESZ - STKSZ;           /*set up return stk pointer*/
        o->S = m + CORESZ - (2*STKSZ);      /*set up variable stk pointer*/
        if(forth_eval(o, initprg) < 0) return NULL; /*define words*/
        forth_seti(o, in);                  /*set up input after out eval*/
        return o;
}

int forth_run(forth_obj_t *o)
{       uint16_t *m, pc, *S, I, f = 0, w;
        if(!o || o->invalid) return WARN("invalid obj"), -1;
        if(setjmp(o->exception)) return -(o->invalid = 1); /*setup handler*/
        m = o->m, S = o->S, I = o->I; /*set S & I to values from forth_init*/

        for(;(pc = m[CK(I++)]);) { /* Threaded code interpreter */
        if((S<m) || (S>(m+CORESZ))) return WARN("stk err."), -(o->invalid = 1);
        INNER:  switch (m[CK(pc++)]) {
                case PUSH:    *++S = f;     f = m[CK(I++)];           break;
                case COMPILE: m[CK(m[0]++)] = pc;                     break;
                case RUN:     m[CK(++m[RSTK])] = I; I = pc;           break;
                case DEFINE:  m[STATE] = 1;
                              if(compile(o, COMPILE, NULL) < 0)
                                      return -(o->invalid = 1) /*EOF*/;
                              m[CK(m[0]++)] = RUN;                    break;
                case IMMEDIATE: *m -= 2; m[m[0]++] = RUN;             break;
                case COMMENT: if(comment(o) < 0) return 0;            break;
                case READ:    m[CK(RSTK)]--;
                              if(ogetwrd(o, o->s) < 1)
                                      return 0;
                              if ((w = find(o)) > 1) {
                                      pc = w + 2;
                                      if (!m[STATE] && m[CK(pc)] == COMPILE)
                                              pc++;
                                      goto INNER;
                              } else if(!isnum((char*)o->s)) {
                                      PWARN(o->s,": not a word or number");
                                      break;
                              }
                              if (m[STATE]) { /*must be a number then*/
                                      m[m[0]++] = 2; /*word push at m[2]*/
                                      m[CK(m[0]++)] = strtol((char*)o->s,0,0);
                              } else {
                                      *++S = f;
                                      f = strtol((char*)o->s, 0, 0);
                              }                                       break;
                case LOAD:    f = m[CK(f)];                           break;
                case STORE:   m[CK(f)] = *S--; f = *S--;              break;
                case SUB:     f = *S-- - f;                           break;
                case ADD:     f = *S-- + f;                           break;
                case AND:     f = *S-- & f;                           break;
                case OR:      f = *S-- | f;                           break;
                case XOR:     f = *S-- ^ f;                           break;
                case INV:     f = ~f;                                 break;
                case SHL:     f = *S-- << f;                          break;
                case SHR:     f = *S-- >> f;                          break;
                case LESS:    f = *S-- < f;                           break;
                case EXIT:    I = m[CK(m[RSTK]--)];                   break;
                case EMIT:    fputc(f, o->out); f = *S--;             break;
                case KEY:     *++S = f; f = ogetc(o);                 break;
                case FROMR:   *++S = f; f = m[CK(m[RSTK]--)];         break;
                case TOR:     m[CK(++m[RSTK])] = f; f = *S--;         break;
                case JMP:     I += m[CK(I)];                          break;
                case JMPZ:    I += f == 0 ? m[I]:1; f = *S--;         break;
                case PNUM:    fprintf(o->out, m[HEX] ? "%X": "%u", f);
                              f = *S--;                               break;
                case QUOTE:   *++S = f;      f = m[CK(I++)];          break;
                case COMMA:   m[CK(m[0]++)] = f; f = *S--;            break;
                case EQUAL:   f = *S-- == f;                          break;
                case SWAP:    w = f;  f = *S--;   *++S = w;           break;
                case DUP:     *++S = f;                               break;
                case DROP:    f = *S--;                               break;
                case OVER:    w = *S; *++S = f; f = w;                break;
                case TAIL:    m[RSTK]--;                              break;
                case BSAVE:   f = blockio(m, *S--, f, 'w');           break;
                case BLOAD:   f = blockio(m, *S--, f, 'r');           break;
                case FIND:    *++S = f;
                              if(ogetwrd(o, o->s) < 1) return 0 /*EOF*/;
                              f = find(o) + 2;
                              f = f < 32 ? 0 : f;                     break;
                case PRINT:   fputs(((char*)m)+f, o->out); f = *S--;  break;
                case PSTK:    if(pstk(o,f,S) < 0) return -1;          break;
                default:      return WARN("illegal op"), -(o->invalid = 1);
                }
        }
        return 0;
}

int main_forth(int argc, char **argv)   /*options: ./forth (-d)? (file)* */
{       int dump = 0, rval = 0, c;      /*dump on?, return value, temp char*/
        FILE *in = NULL, *coreo = NULL; /*current input file, dump file*/
        jmp_buf go;                     /*exception handler state*/
        forth_obj_t *o = NULL;          /*our FORTH environment*/
        if(!(o = forth_init(stdin, stdout))) return -1; /*setup env*/
        if(setjmp(go)) return free(o), -1;     /*setup exception handler*/
        if(argc > 1 && !strcmp(argv[1], "-d")) /*turn core dump on*/
                        argv++, argc--, dump = 1;
        if(argc > 1)
                while(++argv, --argc) {
                        forth_seti(o, in = fopenj(argv[0], "rb", &go));
/*shebang line '#!' */  if((c=fgetc(in)) == '#') comment(o); /*special case*/
                        else if (c == EOF){ fclose(in), in = NULL; continue; }
                        else ungetc(c,in);
                        if((rval = forth_run(o)) < 0) goto END;
                        fclose(in), in = NULL;
                }
        else rval = forth_run(o); /*read from defaults, stdin*/
END:    if(in && (in != stdin)) fclose(in), in = NULL;
        if(dump) { /*perform raw dump of FORTH Virtual Machines memory*/
                if(forth_coredump(o, (coreo = fopenj(coref, "wb", &go))) < 0)
                        return -1;
                fclose(coreo); 
        }
        free(o), o = NULL;
        return rval;
}
