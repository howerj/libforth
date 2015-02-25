#ifndef FORTH
#define FORTH
#ifdef __cplusplus
extern "C" {
#endif
struct forth_obj;
typedef struct forth_obj forth_obj_t;
forth_obj_t *forth_init(FILE * input, FILE * output);
int forth_save(forth_obj_t * tobj, FILE * output);
forth_obj_t *forth_load(FILE * input);
void forth_setin(forth_obj_t * tobj, FILE * input);
void forth_setout(forth_obj_t * tobj, FILE * output);
int forth_run(forth_obj_t * tobj);
#ifdef __cplusplus
}
#endif
#endif
