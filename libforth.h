/** @file       libforth.h
 *  @brief      A FORTH library, based on <www.ioccc.org/1992/buzzard.2.c>
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com **/
#ifndef FORTH_H
#define FORTH_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>

/** @brief **/
struct forth_obj;
typedef struct forth_obj forth_obj_t; 
forth_obj_t *forth_init(FILE * input, FILE * output); 
int forth_run(forth_obj_t *o); 
int forth_eval(forth_obj_t *o, const char *s); 
int forth_coredump(forth_obj_t *o, FILE * dump);
void forth_seti(forth_obj_t *o, FILE * in);     
void forth_seto(forth_obj_t *o, FILE * out);    
void forth_sets(forth_obj_t *o, const char *s); 
int main_forth(int argc, char **argv); 
#ifdef __cplusplus
}
#endif
#endif
