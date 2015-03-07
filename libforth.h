#ifndef FORTH_H
#define FORTH_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
struct forth_obj;
typedef struct forth_obj forth_obj_t; /*a forth environment*/
forth_obj_t *forth_init(FILE * input, FILE * output); /*init an environment*/
int forth_run(forth_obj_t *o); /*run the interpreter in an environment*/
int forth_eval(forth_obj_t *o, const char *s); /*eval a string in environment*/
int forth_coredump(forth_obj_t *o, FILE * dump); /*raw dump of a forth_obj_t*/
void forth_seti(forth_obj_t *o, FILE * in);      /*set input to a file*/
void forth_seto(forth_obj_t *o, FILE * out);     /*set output file*/
void forth_sets(forth_obj_t *o, const char *s);  /*set input to a C-string*/
int main_forth(int argc, char **argv); /*sets up an example interpreter*/
#ifdef __cplusplus
}
#endif
#endif
