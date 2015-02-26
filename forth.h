#ifndef FORTH
#define FORTH
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
struct forth_obj;
typedef struct forth_obj forth_obj_t;

forth_obj_t *forth_init(FILE * input, FILE * output);
int forth_run(forth_obj_t * o);

int forth_coredump(forth_obj_t * o, FILE * dump);

void forth_seti(forth_obj_t * o, FILE * in);
void forth_seto(forth_obj_t * o, FILE * out);
#ifdef __cplusplus
}
#endif
#endif
