/*TODO: eval string, character addressing, put all arrays in same addr space, 
 * bounds check*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "forth.h"

struct forth_obj {
	FILE *input, *output;
	uint8_t s[2500];
	uint16_t m[2500], L, I, T[500], *S, t, w, f, x;
};

#define I tobj->I
#define t tobj->t
#define w tobj->w
#define f tobj->f

enum primitives {
	PUSH, COMPILE, RUN, DEFINE, IMMEDIATE, READ, LOAD, STORE, SUB, ADD,
	MUL, DIV, LESS, EXIT, EMIT, KEY, FROMR, TOR, JMP, JMPZ,
	PRINTNUM, QUOTE, COMMA, NOT, EQUAL, SWAP, DUP, DROP, TAIL
}; /*the order of these matters, related to the names*/

static char *names[] = { "@", "!", "-", "+", "*", "/", "<", "exit", "emit",
"key", "r>", ">r", "jmp",  "jmpz", ".", "'", ",", "not", "=", "swap", "dup",
"drop", "tail", NULL }; /*the order of these matters, related to the enum*/

static void compile_word(forth_obj_t * tobj, unsigned code, bool flag, char *str)
{
	uint16_t *m = tobj->m;
	m[m[0]++] = tobj->L;
	tobj->L = *m - 1;
	m[m[0]++] = t;
	m[m[0]++] = code;
	if (flag)
		strcpy((char *)tobj->s + t, str);
	else
		fscanf(tobj->input, "%s", tobj->s + t);
	t += strlen((char*)tobj->s + t) + 1;
	return;
}

forth_obj_t *forth_init(FILE * input, FILE * output)
{
	uint16_t *m, i;
	forth_obj_t *tobj;
	assert(input && output);

	if(NULL == (tobj = calloc(1, sizeof(*tobj))))
		return NULL;

	m = tobj->m;
	tobj->input = input;
	tobj->output = output;

	m[0] = 32;
	tobj->L = 1;
	I = 0;
	tobj->S = tobj->T;
	t = 64;
	w = f = tobj->x = 0;

	compile_word(tobj, DEFINE,    true, ":");
	compile_word(tobj, IMMEDIATE, true, "immediate");
	compile_word(tobj, COMPILE,   true, "read");
	w = *m;
	m[m[0]++] = READ;
	m[m[0]++] = RUN;
	I = *m;
	m[m[0]++] = w;
	m[m[0]++] = I - 1;
	w = LOAD;

	for(i = 0; names[i]; i++){
		compile_word(tobj, COMPILE, true, names[i]);
		m[m[0]++] = w++;
	}

	m[1] = *m;
	*m += 512;
	return tobj;
}

int forth_run(forth_obj_t * tobj)
{
	uint16_t *m = tobj->m, *x = &tobj->x, *S = tobj->S;
	assert(tobj);
	while (true) { /*add in bounds checking here for all indexes, be conservative*/
		*x = m[I++];
	INNER:
		switch (m[(*x)++]) {
		case PUSH:    *++S = f;      f = m[I++]; break;
		case COMPILE: m[m[0]++] = *x;            break;
		case RUN:     m[++m[1]] = I; I = *x;     break;
		case DEFINE:
			m[8] = 1;
			compile_word(tobj, COMPILE, false, NULL);
			m[m[0]++] = RUN;
			break;
		case IMMEDIATE: *m -= 2; m[m[0]++] = RUN; break;
		case READ:
			m[1]--;
			for (w = fscanf(tobj->input, "%s", tobj->s) < 1 ? exit(0), /*return 0 not exit!*/
			     0 : tobj->L; strcmp((char*)tobj->s, (char*)&tobj->s[m[w + 1]]); w = m[w]) ;
			if (w - 1) {
				*x = w + 2;
				if (m[8] == 0 && m[*x] == COMPILE)
					(*x)++;
				goto INNER;
			} else {
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
		case DIV:   f = *S-- / f;  break; /*XXX: check f>0*/
		case LESS: f = *S-- > f;  break;
		case EXIT:  I = m[m[1]--]; break;
		case EMIT:
			fputc(f, tobj->output); /*add i/o check*/
			f = *S--;
			break;
		case KEY:
			*++S = f;
			f = fgetc(tobj->input); /*add i/o check*/
			break;
		case FROMR:   *++S = f;      f = m[m[1]--]; break;
		case TOR:     m[++m[1]] = f; f = *S--;      break;
		case JMP:     I += m[I];                    break;
		case JMPZ:    I += f == 0 ? m[I] : 1; f = *S--; break;
		case PRINTNUM:
			fprintf(tobj->output, "%u", f);
			f = *S--;
			break;
		case QUOTE: *++S = f;      f = m[I++];    break;
		case COMMA: m[m[0]++] = f; f = *S--;      break;
		case NOT:   f = !f;	break;
		case EQUAL: f = *S-- == f;                break;
		case SWAP:  w = f;  f = *S--;   *++S = w; break;
		case DUP:   *++S = f;                     break;
		case DROP:  f = *S--;                     break;
		case TAIL:  m[1]--;	                  break;
		default:    fputs("Unknown instruction\n", stderr); return -1;
		}
	}
	return -1; /*should not get here*/
}

