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
struct forth_obj; /**An opaque object that holds a running FORTH environment**/
typedef struct forth_obj forth_obj_t;
/** @brief   Given an input and an output this will initialize forth_obj_t,
 *           allocating memory for it and setting it up so it has a few
 *           FORTH words predefined. The returned object must be free'd
 *           by the caller and can be done with free(). It will return NULL
 *           on failure.
 *  @param   input       Read from this input file. Caller closes.
 *  @param   output      Output to this file. Caller closes.
 *  @return  forth_obj_t A fully initialized forth environment. **/
forth_obj_t *forth_init(FILE *input, FILE *output); 
/** @brief   Execute an initialized forth environment, this will read
 *           from input until there is no more or an error occurs. If
 *           an error occurs a negative number will be returned and the
 *           forth object passed to forth_run will be invalidated, all
 *           subsequent calls to forth_run() or forth_eval() will return
 *           errors.
 *  @param   o   An initialized forth environment. Caller frees.
 *  @return  int This is an error code, less than one is an error. **/
int forth_run(forth_obj_t *o); 
/** @brief   This function behaves like forth_run() but instead will
 *           read from a string until there is no more. It will like-
 *           wise invalidate objects if there is an error evaluating the
 *           string. Do not forget to call either forth_seti() or forth_seto(),
 *           or to close any previous files passed to forth_eval() after
 *           you have called forth_eval(). Multiple calls to forth_eval()
 *           will work however.
 *  @param   o   An initialized forth environment. Caller frees.
 *  @param   s   A NUL terminated string to read from.
 *  @return  int This is an error code, less than one is an error. **/
int forth_eval(forth_obj_t *o, const char *s); 
/** @brief   Dump the opaque FORTH object to file, this is a raw dump
 *           of the object and any pointers it may or may not have at
 *           the time of the dump. The contents of this dump are not
 *           guaranteed to have any specific format or structure.
 *  @param   o    The FORTH environment to dump. Caller frees.
 *  @param   dump A file to dump the memory contents to. Caller closes.
 *  @return  int  An error code, negative on error. **/
int forth_coredump(forth_obj_t *o, FILE *dump);
/** @brief Set the input of an environment 'o' to read from a file 'in'.
 *  @param o   An initialized FORTH environment. Caller frees.
 *  @param in  A open file handle for reading; "r" or "rb". Caller closes. **/
void forth_seti(forth_obj_t *o, FILE *in);
/** @brief Set the output file of an environment 'o'.
 *  @param o   An initialized FORTH environment. Caller frees.
 *  @param out A open file handle for writing; "w" or "wb". Caller closes. **/
void forth_seto(forth_obj_t *o, FILE *out);    
/** @brief Set the input of an environment 'o' to read from a string 's'.
 *  @param o   An initialized FORTH environment. Caller frees.
 *  @param s   A NUL terminated string to act as input. **/
void forth_sets(forth_obj_t *o, const char *s); 
/** @brief  This implements a FORTH REPL whose behaviour is documented in
 *          the man pages for this library. You pass in the same format as
 *          is expected to main(). The only option possible to pass to argv
 *          is "-d" which automatically performs a forth_coredump() after
 *          it has read all the files passed in argv. Consult the man pages!
 *  @param  argc  An argument count, like in main().
 *  @param  argv  A number of string arguments, like in main().
 *  @return int   A error code. Anything non zero is an error. **/
int main_forth(int argc, char **argv); 
#ifdef __cplusplus
}
#endif
#endif
