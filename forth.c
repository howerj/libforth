/** @file       forth.c
 *  @brief      A FORTH library, based on <www.ioccc.org/1992/buzzard.2.c>
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com
 *  @todo       Rewrite word header to be more compact.
 *  @todo       Dump registers on error for debugging?
 *  @todo       Experiment with hashing words instead of using names.*/
#include "forth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char *errorfmt = "( error \"%s\" %s %u )\n";
#define WARN(MSG) fprintf(stderr, errorfmt, (MSG), __FILE__, __LINE__)
#define CORESZ    ((UINT16_MAX + 1) / 2)
#define STROFF    (CORESZ/2)
#define STKSZ     (CORESZ/64)
#define BLKSZ     (1024u)
#define CK(X)     ((X) & 0x7fff)
#define HIDE      (0x8000)

struct forth_obj {
        FILE *in, *out;
        uint8_t *s, *sin;
        uint16_t *S, I, t, m[CORESZ];
        size_t slen, sidx;
        unsigned invalid :1, stringin :1;
};

enum registers { DIC = 0/*or m[0]*/, RSTK = 1, STATE = 8, HEX = 9, PWD = 10 };

enum codes { PUSH, COMPILE, RUN, DEFINE, IMMEDIATE, COMMENT, READ, LOAD,
STORE, SUB, ADD, AND, OR, XOR, INV, MUL, DIV, LESS, EXIT, EMIT, KEY, FROMR,
TOR, JMP, JMPZ, PNUM, QUOTE, COMMA, EQUAL, SWAP, DUP, DROP, TAIL, BSAVE,
BLOAD, FIND, LAST };

static char *names[] = { "read", "@", "!", "-", "+", "&", "|", "^", "~", "*",
"/", "<", "exit",  "emit", "key", "r>", ">r", "j",  "jz", ".", "'", ",", "=",
"swap", "dup", "drop", "tail", "save", "load", "find", NULL };

static int ogetc(forth_obj_t * o)
{       if(o->stringin) return o->sidx >= o->slen ? EOF : o->sin[o->sidx++];
        else return fgetc(o->in);
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
        m[m[0]++] = o->t;
        m[m[0]++] = code;
        if (str) strcpy((char *)o->s + o->t, str);
        else     r = ogetwrd(o, o->s + o->t);
        o->t += strlen((char*)o->s + o->t) + 1;
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
                return WARN("could not open file"), -1;
        n = rw == 'w' ? fwrite(p+poffset, 1, BLKSZ, file):
                        fread (p+poffset, 1, BLKSZ, file);
        fclose(file);
        return n == BLKSZ ? 0 : -1;
}

static int isnum(char *s)
{ s += *s == '-' ? 1 : 0; return s[strspn(s,"0123456789")] == '\0'; }

static uint16_t find(forth_obj_t * o)
{       uint16_t *m = o->m, w;
        for (w=m[PWD]; (w & HIDE) || strcmp((char*)o->s, (char*)&o->s[m[(w & ~HIDE) + 1]]); w=m[w & ~HIDE]);
        return w;
}

void forth_seti(forth_obj_t *o, FILE *in)  { o->stringin = 0; o->in  = in;  }
void forth_seto(forth_obj_t *o, FILE *out) { o->stringin = 0; o->out = out; }
void forth_sets(forth_obj_t *o, char *s)
{       o->sidx = 0;
        o->slen = strlen(s);
        o->stringin = 1;
        o->sin = (uint8_t *)s;
}
int forth_eval(forth_obj_t *o, char *s){ forth_sets(o,s); return forth_run(o);}

int forth_coredump(forth_obj_t * o, FILE * dump)
{       if(!o || !dump) return -1;
        return sizeof(*o) == fwrite(o, 1, sizeof(*o), dump);
}


forth_obj_t *forth_init(FILE * in, FILE * out)
{       uint16_t *m, i, w;
        forth_obj_t *o;

        if(!in || !out || !(o = calloc(1, sizeof(*o))))
                return NULL;
        m = o->m;
        o->s = (uint8_t*) m + STROFF; /*string store offset into CORE*/
        o->in = in;
        o->out = out;

        m[0] = 32;   /*initial dictionary offset, skip registers*/
        m[PWD] = 1;
        o->t = 32;   /*offset into str storage defines maximum word length*/

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
        return o;
}

int forth_run(forth_obj_t * o)
{       int c;
        uint16_t *m, pc, *S, I, f, w;

        if(!o || o->invalid) return WARN("invalid obj"), -1;
        m = o->m, S = o->S, I = o->I;

        for(;(pc = m[I++]);) {
        INNER:  switch (m[pc++]) {
                case PUSH:    *++S = f;     f = m[I++];     break;
                case COMPILE: m[m[0]++] = pc;               break;
                case RUN:     m[++m[RSTK]] = I; I = pc;     break;
                case DEFINE:  m[STATE] = 1;
                              if(compile(o, COMPILE, NULL) < 0)
                                      return -(o->invalid = 1);
                              m[m[0]++] = RUN;              break;
                case IMMEDIATE: *m -= 2; m[m[0]++] = RUN;   break;
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
                                      WARN("not a word or number");
                                      break;
                              }
                              if (m[STATE]) { /*must be a number then*/
                                      m[m[0]++] = 2; /*fake word push at m[2]*/
                                      m[m[0]++] = strtol((char*)o->s, NULL, 0);
                              } else {
                                      *++S = f;
                                      f = strtol((char*)o->s, NULL, 0);
                              }
                              break;
                case LOAD:    f = m[CK(f)];                   break;
                case STORE:   m[CK(f)] = *S--; f = *S--;      break;
                case SUB:     f = *S-- - f;                   break;
                case ADD:     f = *S-- + f;                   break;
                case AND:     f = *S-- & f;                   break;
                case OR:      f = *S-- | f;                   break;
                case XOR:     f = *S-- ^ f;                   break;
                case INV:     f = ~f;                         break;
                case MUL:     f *= *S--;                      break;
                case DIV:     f = f ? *S--/f:WARN("div 0"),0; break;
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
                default:      return WARN("illegal op"), -(o->invalid = 1);
                }
        }
        return 0;
}

