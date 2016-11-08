/** 
% LIBFORTH(3)
% Richard Howe
% November 2016

@file       libforth.h
@brief      A FORTH library
@author     Richard James Howe.
@copyright  Copyright 2015,2016 Richard James Howe.
@license    MIT 
@email      howe.r.j.89@gmail.com 
@todo       Generate the manual page for the library from this header 
**/
#ifndef FORTH_H
#define FORTH_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

/**
@brief This is the absolute minimum size the Forth virtual machine can be in
Forth cells, not bytes. 
**/
#define MINIMUM_CORE_SIZE (2048)

struct forth; /**< An opaque object that holds a running FORTH environment**/
typedef struct forth forth_t;
typedef uintptr_t forth_cell_t; /**< FORTH cell large enough for a pointer*/

#define PRIdCell PRIdPTR /**< Decimal format specifier for a Forth cell */
#define PRIxCell PRIxPTR /**< Hex format specifier for a Forth word */

typedef int (*forth_function_t)(forth_t *o);

/**
@brief struct forth_functions allows arbitrary C functions to be passed
to the forth interpreter which can be used from within the Forth interpreter.

This structure can be used to extend the forth interpreter with functions
defined elsewhere, which is particularly useful for allowing the interpreter
to use non-portable functions.

@todo rewrite structure so static allocation of functions can be done,
thus removing the need for **forth\_new\_function\_list** and 
**forth\_delete\_function\_list**.

**/
struct forth_functions
{
	forth_cell_t count; /**< number of functions */
	struct forth_function {
		unsigned depth; /**< depth expected on stack before call */
		forth_function_t function; /**< function to execute */
	} functions[]; /**< list of possible functions for CALL */
};

/**
@brief   Given an input and an output this will initialize forth,
allocating memory for it and setting it up so it has a few
FORTH words predefined. The returned object must be freed
by the caller and can be done with forth_free(). It will return 
NULL on failure.

@param   size    Size of interpreter environment, must be greater 
or equal to MINIMUM_CORE_SIZE
@param   input   Read from this input file. Caller closes.
@param   output  Output to this file. Caller closes.
@param   calls   Used to specify arbitrary functions that the interpreter 
can call Can be NULL, caller frees if allocated.
@return  forth A fully initialized forth environment or NULL. 
**/
forth_t *forth_init(size_t size, FILE *input, FILE *output, 
		const struct forth_functions *calls); 

/**
@brief   Given a FORTH object it will free any memory and perform any
internal cleanup needed. This will not free any evaluated
strings nor will it close any files passed in via the C-API.

@param   o    An object to free, Asserted 
**/
void forth_free(forth_t *o); 

/**
@brief Allocate space for a function list.
@param count the number of functions that can be held.
@return a function list that can be passed to forth_init, or NULL on
failure.
**/
struct forth_functions *forth_new_function_list(forth_cell_t count);

/**
@brief Free a function list.
@param calls function list to free.
**/
void forth_delete_function_list(struct forth_functions *calls);

/** 
@brief  find a forth word in its dictionary if it exists, there must
be no extra characters (apart from a terminating NUL) in the
word name, the entire string will be searched for.

@param  o initialized forth environment
@param  s a string, representing a words name, to find
@return non zero if the word has been found, zero if it has not been
**/
forth_cell_t forth_find(forth_t *o, const char *s);

/** 
@brief  push a value onto the variable stack

@param  o initialized forth environment
@param  f value to push 
**/
void forth_push(forth_t *o, forth_cell_t f);

/** 
@brief  pop a value from the variable stack

@param  o initialized forth environment
@return popped value 
**/
forth_cell_t forth_pop(forth_t *o);

/** 
@brief  get the current stack position

@param  o initialized forth environment
@return stack position, number of items on the stack 
**/
forth_cell_t forth_stack_position(forth_t *o);

/** 
@brief   Execute an initialized forth environment, this will read
from input until there is no more or an error occurs. If
an error occurs a negative number will be returned and the
forth object passed to forth_run will be invalidated, all
subsequent calls to forth_run() or forth_eval() will return
errors.

@param   o   An initialized forth environment. Caller frees.
@return  int This is an error code, less than one is an error. 
**/
int forth_run(forth_t *o); 

/** 
@brief   This function behaves like forth_run() but instead will
read from a string until there is no more. It will like-
wise invalidate objects if there is an error evaluating the
string. Do not forget to call either forth_set_file_input() or 
forth_set_string_input() or to close any previous files 
passed to forth\_eval() after you have called forth\_eval(). 
Multiple calls to forth\_eval()
will work however.

@param   o   An initialized forth environment. Caller frees.
@param   s   A NUL terminated string to read from.
@return  int This is an error code, less than one is an error. 
**/
int forth_eval(forth_t *o, const char *s); 

/** 
@brief  Dump a raw forth object to disk, for debugging purposes, this
cannot be loaded with "forth\_load\_core".

@param  o    forth object to dump, caller frees, asserted.
@param  dump file to dump to (opened as "wb"), caller frees, asserted.
@return int 0 if successful, non zero otherwise 
**/
int forth_dump_core(forth_t *o, FILE *dump);

/** 
@brief   Save the opaque FORTH object to file, this file may be
loaded again with forth\_load\_core. The file passed in should
be have been opened up in binary mode ("wb"). These files
are not portable, files generated on machines with different
machine word sizes or endianess will not work with each
other.

@warning Note that this function will not save out the contents
or in anyway remember the forth_functions structure passed
in to forth_init, it is up to the user to correctly pass in
the right value after loading a previously saved core. Therefore
portable core files must be generated from a forth_init that was
passed NULL.

@param   o    The FORTH environment to dump. Caller frees. Asserted.
@param   dump Core dump file handle ("wb"). Caller closes. Asserted.
@return  int  An error code, negative on error. 
**/
int forth_save_core(forth_t *o, FILE *dump);

/** 
@brief  Load a Forth file from disk, returning a forth object that
can be passed to forth_run.

@param  dump    a file handle opened on a Forth core dump, previously
saved with forth_save_core, this must be opened
in binary mode ("rb").
@return forth_t a reinitialized forth object, or NULL on failure
**/
forth_t *forth_load_core(FILE *dump);

/** 
@brief   Define a new constant in an Forth environment.

@param   o    Forth environment to define new constant in
@param   name Name of constant, should be less than 31 characters in
length as only they will be used in defining the new
name
@param   c    Value of constant
@return  Same return status as forth\_eval 
**/
int forth_define_constant(forth_t *o, const char *name, forth_cell_t c);

/** 
@brief Set the input of an environment 'o' to read from a file 'in'.

@param o   An initialized FORTH environment. Caller frees.
@param in  Open handle for reading; "r"/"rb". Caller closes. 
**/
void forth_set_file_input(forth_t *o, FILE *in);

/** 
@brief Set the output file of an environment 'o'.

@param o   An initialized FORTH environment. Caller frees. Asserted.
@param out Open handle for writing; "w"/"wb". Caller closes. Asserted. 
**/
void forth_set_file_output(forth_t *o, FILE *out);

/** 
@brief Set the input of an environment 'o' to read from a string 's'.

@param o   An initialized FORTH environment. Caller frees. Asserted.
@param s   A NUL terminated string to act as input. Asserted. 
**/
void forth_set_string_input(forth_t *o, const char *s); 

/** 
@brief Set the register elements in the Forth virtual machine for
"argc" and "argv" to argc and argv, allowing them to be
accessible within the interpreter

@param o    An initialized FORTH environment. Caller frees. Asserted.
@param argc argc, as is passed into main()
@param argv argv, as is passed into main() 
**/
void forth_set_args(forth_t *o, int argc, char **argv);

/** 
@brief  This implements a FORTH REPL whose behavior is documented in
the man pages for this library. You pass in the same format as
is expected to main(). The only option possible to pass to argv
is "-d" which automatically performs a forth\_coredump() after
it has read all the files passed in argv. All other strings
are treated as file names to open and read input from, apart
from "-", which sets the interpreter to read from stdin. Consult
the man pages.

Currently there is no mechanism for passing a struct forth\_functions
to this call, this is deliberate. A saved Forth file will not make
sense without the correct forth\_functions structure and associated
functions.

@param  argc  An argument count, like in main().
@param  argv  argc strings, like in main(). Not checked for NULL.
@return int   A error code. Anything non zero is an error. 
**/
int main_forth(int argc, char **argv); 

#ifdef __cplusplus
}
#endif
#endif
