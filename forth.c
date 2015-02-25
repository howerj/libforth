/*TODO: eval string, character addressing, put all arrays in same addr space, 
 * bounds check*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "forth.h"

#define WARN(MSG) fprintf(stderr,"(error \"%s\" %s %d)\n", (MSG), __FILE__, __LINE__)
#define CORESZ    ((UINT16_MAX + 1) / 2)
#define STROFF    (CORESZ/2)
#define STKSZ     (512u)
#define BLKSZ     (1024u)

struct forth_obj {
        FILE *input, *output;
        uint8_t *s;
        uint16_t m[CORESZ], *S, L, I, t, w, f, x;
        unsigned initialized :1;
        unsigned invalidated :1;
};

#define t tobj->t
#define w tobj->w
#define f tobj->f

enum primitives {
        PUSH, COMPILE, RUN, DEFINE, IMMEDIATE, READ, LOAD, STORE, SUB, ADD,
        MUL, DIV, LESS, EXIT, EMIT, KEY, FROMR, TOR, JMP, JMPZ, PRINTNUM, 
        QUOTE, COMMA, NOT, EQUAL, SWAP, DUP, DROP, TAIL, BLOAD, BSAVE, LAST
}; 

static char *names[] = { "@", "!", "-", "+", "*", "/", "<", "exit", "emit",
"key", "r>", ">r", "jmp",  "jmpz", ".", "'", ",", "not", "=", "swap", "dup",
"drop", "tail", "load", "save", NULL }; 

static void compile_word(forth_obj_t * tobj, unsigned code, bool flag, char *str)
{
        uint16_t *m;
        assert(tobj && (!flag || str));
        m = tobj->m;
        m[m[0]++] = tobj->L;
        tobj->L = *m - 1;
        m[m[0]++] = t;
        m[m[0]++] = code;
        if (flag)
                strcpy((char *)tobj->s + t, str);
        else
                fscanf(tobj->input, "%31s", tobj->s + t);
        t += strlen((char*)tobj->s + t) + 1;
        return;
}

static int blockio(void *p, uint16_t poffset, uint16_t id, char rw){
        char name[16];
        FILE *file;
        int r = 0;
        size_t n;
        if(poffset > (CORESZ - BLKSZ) /*|| !(rw == 'w' || rw == 'r')*/){
                puts("failed");
                return -1;
        }
        sprintf(name, "%04x.blk", id);
        if(NULL == (file = fopen(name, rw == 'r' ? "rb" : "wb")))
                return -1;
        if (rw == 'w')
                n = fwrite(p+poffset, 1, BLKSZ, file);
        else 
                n = fread(p+poffset, 1, BLKSZ, file);
        r = n == BLKSZ ? 0 : -1;
        fclose(file);
        return r;
}

void forth_setin(forth_obj_t * tobj, FILE * input){
        assert(tobj && input);
        tobj->input = input;
}

void forth_setout(forth_obj_t * tobj, FILE * output){
        assert(tobj && output);
        tobj->output = output;
}

int forth_save(forth_obj_t * tobj, FILE * output){ /*XXX: Does not work yet*/
        if(!tobj || !output) return -1;
        return sizeof(*tobj) == fwrite(tobj, 1, sizeof(*tobj), output);
}

forth_obj_t *forth_load(FILE * input){ /*XXX: Does not work yet*/
        forth_obj_t *tobj;
        if(!input) return NULL;
        if(NULL == (tobj = calloc(1, sizeof(*tobj))))
                return NULL;
        if(sizeof(*tobj) == fread(tobj, 1, sizeof(*tobj), input))
                return tobj;
        return NULL;
}

forth_obj_t *forth_init(FILE * input, FILE * output)
{
        uint16_t *m, i;
        forth_obj_t *tobj;
        assert(input && output);

        if(NULL == (tobj = calloc(1, sizeof(*tobj))))
                return NULL;
        m = tobj->m;
        tobj->s = (uint8_t*) m + STROFF; /*string store offset into CORE*/
        tobj->input = input;
        tobj->output = output;

        m[0] = 32;   /*initial dictionary offset*/
        tobj->L = 1; 
        t = 32;      /*offset into str storage defines maxium word length*/

        compile_word(tobj, DEFINE,    true, ":");
        compile_word(tobj, IMMEDIATE, true, "immediate");
        compile_word(tobj, COMPILE,   true, "read");
        w = *m;
        m[m[0]++] = READ;
        m[m[0]++] = RUN;
        tobj->I = *m;
        m[m[0]++] = w;
        m[m[0]++] = tobj->I - 1;
        w = LOAD;

        for(i = 0; names[i]; i++)
                compile_word(tobj, COMPILE, true, names[i]), m[m[0]++] = w++;

        m[1] = *m;                /*setup return stack after compiled words*/
        tobj->S = m + *m + STKSZ; /*var stack after that*/
        *m += (2*STKSZ);          /*continue the dictionary after that*/
        return tobj;
}

int forth_run(forth_obj_t * tobj)
{
        uint16_t *m, x, *S, I;

        if(NULL == tobj || tobj->invalidated) return -(tobj->invalidated = 1);
        m = tobj->m, x = tobj->x, S = tobj->S, I = tobj->I;
        tobj->initialized = 1;

        while (true) { /*add in bounds checking here for all indexes, be conservative*/
                x = m[I++];
        INNER:
                switch (m[x++]) {
                case PUSH:    *++S = f;     f = m[I++]; break;
                case COMPILE: m[m[0]++] = x;            break;
                case RUN:     m[++m[1]] = I; I = x;     break;
                case DEFINE:
                        m[8] = 1;
                        compile_word(tobj, COMPILE, false, NULL);
                        m[m[0]++] = RUN;
                        break;
                case IMMEDIATE: *m -= 2; m[m[0]++] = RUN; break;
                case READ: /*should check if it is number, else fail*/
                        m[1]--;
                        if(fscanf(tobj->input, "%31s", tobj->s) < 1)
                                return 0;
                        for (w = tobj->L; strcmp((char*)tobj->s, (char*)&tobj->s[m[w + 1]]); w = m[w]) ;
                        if (w - 1) {
                                x = w + 2;
                                if (m[8] == 0 && m[x] == COMPILE)
                                        x++;
                                goto INNER;
                        } else {
                                if(strspn((char*)tobj->s,"0123456789") != strlen((char*)tobj->s)){
                                        fprintf(stderr,"(error \"%s: not a word or number\")\n", tobj->s);
                                        break;
                                }

                                if (m[8] != 0) {
                                        m[m[0]++] = 2;
                                        m[m[0]++] = strtol((char*)tobj->s, NULL, 0);
                                } else {
                                        *++S = f;
                                        f = strtol((char*)tobj->s, NULL, 0);
                                }
                        }
                        break;
                case LOAD:  f = m[f];      break;
                case STORE: m[f] = *S--;   f = *S--; break;
                case SUB:   f = *S-- - f;  break;
                case ADD:   f = *S-- + f;  break;
                case MUL:   f *= *S--;     break;
                case DIV:
                        if(f) f = *S-- / f;  
                        else  f = *S--, WARN("div 0");
                        break;
                case LESS:  f = *S-- > f;  break;
                case EXIT:  I = m[m[1]--]; break;
                case EMIT:  /*what should we do if fputc fails?*/
                        fputc(f, tobj->output); 
                        f = *S--;
                        break;
                case KEY:
                        *++S = f;
                        f = fgetc(tobj->input); 
                        break;
                case FROMR:   *++S = f;      f = m[m[1]--]; break;
                case TOR:     m[++m[1]] = f; f = *S--;      break;
                case JMP:     I += m[I];           break;
                case JMPZ:    I += f == 0 ? m[I] : 1; f = *S--; break;
                case PRINTNUM:
                        fprintf(tobj->output, "%u", f);
                        f = *S--;
                        break;
                case QUOTE: *++S = f;      f = m[I++];    break;
                case COMMA: m[m[0]++] = f; f = *S--;      break;
                case NOT:   f = !f;        break;
                case EQUAL: f = *S-- == f;                break;
                case SWAP:  w = f;  f = *S--;   *++S = w; break;
                case DUP:   *++S = f;                     break;
                case DROP:  f = *S--;                     break;
                case TAIL:  m[1]--;                       break;
                case BLOAD: f = blockio(m,*S--,f,'r');    break;
                case BSAVE: f = blockio(m,*S--,f,'w');    break;
                default:
                            WARN("unknown instruction");
                            return -(tobj->invalidated = 1);
                }
        }
        WARN("reached the unreachable");
        return -(tobj->invalidated = 1); /*should not get here*/
}

