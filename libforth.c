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

static const char *coref = "forth.core";
static const char *forth_start = "\
: state 8 ! exit \
: ; immediate ' exit , 0 state exit \
: hex 9 ! ; : pwd 10 ; : h 0 ; : r 1 ; \
: here h @ ; \
: [ immediate 0 state ; : ] 1 state ; \
: :noname immediate here 2 , ] ; \
: if immediate ' jz , here 0 , ; \
: else immediate ' j , here 0 , swap dup here swap - swap ! ; \
: then immediate dup here swap - swap ! ; \
: begin immediate here ; \
: until immediate ' jz , here - , ; \
: 0= 0 = ; : 1+ 1 + ; : ')' 41 ; : tab 9 emit ; : cr 10 emit ; \
: .( key drop begin key dup ')' = if drop exit then emit 0 until ; \
: 2dup dup >r >r dup r> swap r> ; \
: line dup . tab dup 4 + swap begin dup @ . tab 1+ 2dup = until drop ; \
: list swap begin line cr 2dup < until ; \
: allot here + h ! ; \
: words pwd @ begin dup 1 + @ 32768 + print tab @ dup 32 < until ; \n\
: :: [ find : , ] ; \
: create :: 2 , here 2 + , ' exit , 0 state ; ";

static const char *errorfmt = "( error \"%s%s\" %s %u )\n";
#define PWARN(P,M) fprintf(stderr, errorfmt, (P), (M), __FILE__, __LINE__)
#define WARN(M)    PWARN("",(M))
#define CORESZ     ((UINT16_MAX + 1) / 2)
#define STROFF     (CORESZ/2)
#define STKSZ      (CORESZ/64)
#define BLKSZ      (1024)
#define CK(X)      ((X) & 0x7fff)
#define HIDE       (0x8000)

struct forth_obj {
        uint16_t m[CORESZ], *S, I;
        FILE *in, *out;
        uint8_t *s, *sin;
        size_t slen, sidx;
        unsigned invalid :1, stringin :1;
};

enum registers { DIC=0/*m[0]*/, RSTK=1, STATE=8, HEX=9, PWD=10, SSTORE=11 };

enum codes { PUSH, COMPILE, RUN, DEFINE, IMMEDIATE, COMMENT, READ, LOAD,
STORE, SUB, ADD, AND, OR, XOR, INV, SHL, SHR, LESS, EXIT, EMIT, KEY, FROMR,
TOR, JMP, JMPZ, PNUM, QUOTE, COMMA, EQUAL, SWAP, DUP, DROP, TAIL, BSAVE,
BLOAD, FIND, PRINT, LAST };

static char *names[] = { "read", "@", "!", "-", "+", "&", "|", "^", "~", "<<",
">>", "<", "exit",  "emit", "key", "r>", ">r", "j",  "jz", ".", "'", ",", "=",
"swap", "dup", "drop", "tail", "save", "load", "find", "print", NULL };

static int ogetc(forth_obj_t * o)
{       if(o->stringin) return o->sidx >= o->slen ? EOF : o->sin[o->sidx++];
        else return fgetc(o->in);
}

static FILE *_fopen_or_fail(const char *name, char *mode)
{       FILE *file = fopen(name, mode);
        if(!file) perror(name), exit(1);
        return file;
}

static int ogetwrd(forth_obj_t * o, uint8_t *p)
{       int n = 0;
        if(o->stringin){
                if(sscanf((char *)&(o->sin[o->sidx]), "%31s%n", p, &n) < 0)
                        return EOF;
                o->sidx += n;
                return n;
        } else {
                return fscanf(o->in, "%31s", p);
        }
}

static int compile(forth_obj_t * o, uint16_t code, char *str)
{       uint16_t *m;
        int r = 0;
        m = o->m;
        m[m[0]++] = m[PWD];
        m[PWD] = *m - 1;
        m[m[0]++] = m[SSTORE];
        m[m[0]++] = code;
        if (str) strcpy((char *)o->s + m[SSTORE], str);
        else     r = ogetwrd(o, o->s + m[SSTORE]);
        m[SSTORE] += strlen((char*)o->s + m[SSTORE]) + 1;
        return r;
}

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
}

static int isnum(char *s)
{ s += *s == '-' ? 1 : 0; return s[strspn(s,"0123456789")] == '\0'; }

static uint16_t find(forth_obj_t * o)
{       uint16_t *m = o->m, w;
        w = m[PWD];
        for (;(w & HIDE)||strcmp((char*)o->s, (char*)&o->s[m[(w & ~HIDE)+1]]);)
                w = m[w & ~HIDE];

        return w;
}

void forth_seti(forth_obj_t *o, FILE *in)  { o->stringin = 0; o->in  = in;  }
void forth_seto(forth_obj_t *o, FILE *out) { o->stringin = 0; o->out = out; }
void forth_sets(forth_obj_t *o, const char *s)
{       o->sidx = 0;
        o->slen = strlen(s) + 1;
        o->stringin = 1;
        o->sin = (uint8_t *)s;
}
int forth_eval(forth_obj_t *o, const char *s){ forth_sets(o,s); return forth_run(o);}

int forth_coredump(forth_obj_t * o, FILE * dump)
{       if(!o || !dump) return -1;
        return sizeof(*o) == fwrite(o, 1, sizeof(*o), dump) ? 0 : -1;
}

forth_obj_t *forth_init(FILE * in, FILE * out)
{       uint16_t *m, i, w;
        forth_obj_t *o;

        if(!in || !out || !(o = calloc(1, sizeof(*o)))) return NULL;
        m = o->m;
        o->s = (uint8_t*)(m + STROFF); /*string store offset into CORE*/
        o->out = out;

        m[0] = 32;      /*initial dictionary offset, skip registers*/
        m[PWD] = 1;
        m[SSTORE] = 32; /*offset into str storage defines maximum word length*/

        w = *m;
        m[m[0]++] = READ; /*create a special word that reads in*/
        m[m[0]++] = RUN;  /*call the special word recursively*/
        o->I = *m;
        m[m[0]++] = w;
        m[m[0]++] = o->I - 1;

        compile(o, DEFINE,    ":");
        compile(o, IMMEDIATE, "immediate");
        compile(o, COMMENT,   "#");
        for(i = 0, w = READ; names[i]; i++)
                compile(o, COMPILE, names[i]), m[m[0]++] = w++;
        m[RSTK] = CORESZ - STKSZ;
        o->S = m + CORESZ - (2*STKSZ);
        if(forth_eval(o, forth_start) < 0) return NULL;
        forth_seti(o, in);
        return o;
}

int forth_run(forth_obj_t *o)
{       int c;
        uint16_t *m, pc, *S, I, f = 0, w;
        if(!o || o->invalid) return WARN("invalid obj"), -1;
        m = o->m, S = o->S, I = o->I;

        for(;(pc = m[CK(I++)]);) {
        INNER:  switch (m[CK(pc++)]) {
                case PUSH:    *++S = f;     f = m[I++];       break;
                case COMPILE: m[CK(m[0]++)] = pc;             break;
                case RUN:     m[CK(++m[RSTK])] = I; I = pc;   break;
                case DEFINE:  m[STATE] = 1;
                              if(compile(o, COMPILE, NULL) < 0)
                                      return -(o->invalid = 1);
                              m[CK(m[0]++)] = RUN;            break;
                case IMMEDIATE: *m -= 2; m[m[0]++] = RUN;     break;
                case COMMENT: while (((c=ogetc(o)) > 0) && (c != '\n'));
                                                              break;
                case READ:    m[RSTK]--;
                              if(ogetwrd(o, o->s) < 1)
                                      return 0;
                              if ((w = find(o)) > 1) {
                                      pc = w + 2;
                                      if (!m[STATE] && m[pc] == COMPILE)
                                              pc++;
                                      goto INNER;
                              } else if(!isnum((char*)o->s)) {
                                      PWARN(o->s,": not a word or number");
                                      break;
                              }
                              if (m[STATE]) { /*must be a number then*/
                                      m[m[0]++] = 2; /*fake word push at m[2]*/
                                      m[m[0]++] = strtol((char*)o->s, NULL, 0);
                              } else {
                                      *++S = f;
                                      f = strtol((char*)o->s, NULL, 0);
                              }                               break;
                case LOAD:    f = m[CK(f)];                   break;
                case STORE:   m[CK(f)] = *S--; f = *S--;      break;
                case SUB:     f = *S-- - f;                   break;
                case ADD:     f = *S-- + f;                   break;
                case AND:     f = *S-- & f;                   break;
                case OR:      f = *S-- | f;                   break;
                case XOR:     f = *S-- ^ f;                   break;
                case INV:     f = ~f;                         break;
                case SHL:     f = *S-- << f;                  break;
                case SHR:     f = *S-- >> f;                  break;
                case LESS:    f = *S-- < f;                   break;
                case EXIT:    I = m[m[RSTK]--];               break;
                case EMIT:    fputc(f, o->out); f = *S--;     break;
                case KEY:     *++S = f; f = ogetc(o);         break;
                case FROMR:   *++S = f; f = m[m[RSTK]--];     break;
                case TOR:     m[++m[RSTK]] = f; f = *S--;     break;
                case JMP:     I += m[I];                      break;
                case JMPZ:    I += f == 0 ? m[I]:1; f = *S--; break;
                case PNUM:    fprintf(o->out, m[HEX]? "%X":"%u", f);
                              f = *S--;                       break;
                case QUOTE:   *++S = f;      f = m[I++];      break;
                case COMMA:   m[m[0]++] = f; f = *S--;        break;
                case EQUAL:   f = *S-- == f;                  break;
                case SWAP:    w = f;  f = *S--;   *++S = w;   break;
                case DUP:     *++S = f;                       break;
                case DROP:    f = *S--;                       break;
                case TAIL:    m[RSTK]--;                      break;
                case BSAVE:   f = blockio(m,*S--,f,'w');      break;
                case BLOAD:   f = blockio(m,*S--,f,'r');      break;
                case FIND:    *++S = f;
                              if(ogetwrd(o, o->s) < 1) return 0;
                              f = find(o) + 2;
                              f = f < 32 ? 0 : f;             break;
                case PRINT:   fputs(((char*)m)+f, o->out);
                              f = *S--;                       break;
                default:      return WARN("illegal op"), -(o->invalid = 1);
                }
        }
        return 0;
}

int main_forth(int argc, char **argv) 
{       int dump = 0, rval = 0;
        FILE *in = NULL, *coreo = NULL; 
        forth_obj_t *o;
        if(!(o = forth_init(stdin, stdout))) return -1;
        if(argc > 1)
                if(!strcmp(argv[1], "-d"))
                        argv++, argc--, dump = 1;
        if(argc > 1) {
                while(++argv, --argc) {
                        forth_seti(o, in = _fopen_or_fail(argv[0], "rb"));
                        if(forth_run(o) < 0) {
                                rval = -1;
                                goto END;
                        }
                        fclose(in), in = NULL;
                }
        } else if (forth_run(o) < 0) {
                rval = -1;
        }
END:    if(in) fclose(in);
        if(dump) {
                if(forth_coredump(o, (coreo = _fopen_or_fail(coref, "wb"))) < 0)
                        return -1;
                fclose(coreo); 
        }
        free(o);
        return rval;
}
