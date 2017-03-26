/** 
% LIBFORTH(3)
% Richard Howe
% November 2017

@file       libforth.h
@brief      A FORTH library
@author     Richard James Howe.
@copyright  Copyright 2015,2016,2017 Richard James Howe.
@license    MIT 
@email      howe.r.j.89@gmail.com 

@todo Some arguments accept their size in bytes, others in forth cells, this 
needs to be changed so it is consistent
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

/**
@brief Default VM size which should be large enough for any Forth application,
in Forth cells, not bytes.
**/
#ifndef DEFAULT_CORE_SIZE
#define DEFAULT_CORE_SIZE   (32 * 1024) 
#endif

/**
@brief When designing a binary format, which this interpreter uses and
saves to disk, it is imperative that certain information is saved to
disk - one of those pieces of information is the version of the
interpreter. This value is used for compatibility checking. Each version
is incompatible with previous or later versions, which is a deficiency of the
program. A way to migrate core files would be useful, but the task is
too difficult.
**/
#define FORTH_CORE_VERSION  (0x04u)

struct forth; /**< An opaque object that holds a running FORTH environment**/
typedef struct forth forth_t; /**< Typedef of opaque object for general use */
typedef uintptr_t forth_cell_t; /**< FORTH cell large enough for a pointer*/

#define PRIdCell PRIdPTR /**< Decimal format specifier for a Forth cell */
#define PRIxCell PRIxPTR /**< Hex format specifier for a Forth word */

/**
@brief The **IS_BIG_ENDIAN** macro looks complicated, however all it does is
determine the endianess of the machine using trickery.

See:

* <https://stackoverflow.com/questions/2100331>
* <https://en.wikipedia.org/wiki/Endianness>

For more information and alternatives.

**/
#define IS_BIG_ENDIAN (!(union { uint16_t u16; uint8_t c; }){ .u16 = 1 }.c)

/**
@brief Functions matching this typedef can be called via the CALL instruction.
**/
typedef int (*forth_function_t)(forth_t *o);

/**
@brief struct forth_functions allows arbitrary C functions to be passed
to the forth interpreter which can be used from within the Forth interpreter.

This structure can be used to extend the forth interpreter with functions
defined elsewhere, which is particularly useful for allowing the interpreter
to use non-portable functions.
**/
struct forth_functions
{
	forth_cell_t count; /**< number of functions */
	/**@brief The only information needed to perform a CALL is the function
	 * that needs calling and the depth expected on the call stack. This
	 * interface is minimal, but works. */
	struct forth_function {
		unsigned depth; /**< depth expected on stack before call */
		forth_function_t function; /**< function to execute */
	} *functions; /**< list of possible functions for CALL */
};

/**
@brief The logging function is used to print error messages,
warnings and notes within this program.
@param prefix prefix to add to any logged messages
@param func   function in which logging function is called
@param line   line number logging function was called at
@param fmt    logging format string
@param ...    arguments for format string
@return int < 0 is failure
**/
int forth_logger(const char *prefix, const char *func, 
		unsigned line, const char *fmt, ...);

/**
Some macros are also needed for logging. As an aside, **__VA_ARGS__** should 
be prepended with '##' in case zero extra arguments are passed into the 
variadic macro, to swallow the extra comma, but it is not *standard* C, even
if most compilers support the extension.
**/

/**
@brief Variadic macro for handling fatal error printing information
@note This function does not terminate the process, it is up to the user to
this after fatal() is called. 
@param FMT printf style format string 
**/
#define fatal(FMT,...)   forth_logger("fatal",  __func__, __LINE__, FMT, __VA_ARGS__)

/**
@brief Variadic macro for handling error printing information
@note Use this for recoverable errors
@param FMT printf style format string 
**/
#define error(FMT,...)   forth_logger("error",  __func__, __LINE__, FMT, __VA_ARGS__)

/**
@brief Variadic macro for handling warnings
@note Use this for minor problems, for example, some optional component failed.
@param FMT printf style format string 
**/
#define warning(FMT,...) forth_logger("warning",__func__, __LINE__, FMT, __VA_ARGS__)

/**
@brief Variadic macro for notes
@note Use this printing high-level information about the interpreter, for example
opening up a new file. It should be used sparingly.
@param FMT printf style format string 
**/
#define note(FMT,...)    forth_logger("note",   __func__, __LINE__, FMT, __VA_ARGS__)

/**
@brief Variadic macro for debugging information.
@note Use this for debugging, debug messages can be subject to change and may be
present or removed arbitrarily between releases. This macro can be used liberally.
@warning May produce copious amounts of output.
@param FMT printf style format string 
**/
#define debug(FMT,...)   forth_logger("debug",  __func__, __LINE__, FMT, __VA_ARGS__)

/**
@brief These are the possible options for the debug registers. Higher levels
mean more verbose error messages are generated.
**/
enum forth_debug_level
{
	FORTH_DEBUG_OFF,         /**< tracing is off */
	FORTH_DEBUG_FORTH_CODE,  /**< used within the forth interpreter */
	FORTH_DEBUG_NOTE,        /**< print notes */
	FORTH_DEBUG_INSTRUCTION, /**< instructions and stack are traced */
	FORTH_DEBUG_CHECKS,      /**< bounds checks are printed out */
	FORTH_DEBUG_ALL,         /**< trace everything that can be traced */
};

/**
@brief Compute the binary logarithm of an integer value
@param  x number to act on
@return log2 of x
**/
forth_cell_t forth_blog2(forth_cell_t x);

/**
@brief Round up a number to the nearest power of 2
@param  r number to round up
@return rounded up number
**/
forth_cell_t forth_round_up_pow2(forth_cell_t r);

/**
@brief   Given an input and an output this will initialize forth,
allocating memory for it and setting it up so it has a few
FORTH words predefined. The returned object must be freed
by the caller and can be done with forth_free(). It will return 
NULL on failure.

@param   size    Size of interpreter environment, must be greater 
or equal to MINIMUM_CORE_SIZE
@param   in      Read from this input file. Caller closes.
@param   out     Output to this file. Caller closes.
@param   calls   Used to specify arbitrary functions that the interpreter 
can call Can be NULL, caller frees if allocated.
@return  forth A fully initialized forth environment or NULL. 
**/
forth_t *forth_init(size_t size, FILE *in, FILE *out, 
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
@brief Convert a string, representing a numeric value, into a forth cell.
@param  base base to convert string from, valid values are 0, and 2-26
@param[out]  n the result of the conversion is stored here
@param  s    string to convert
@return int return code indicating failure (non zero) or success (zero)
**/
int forth_string_to_cell(int base, forth_cell_t *n, const char *s);

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
@brief Alert a Forth environment to a signal, this function should be
called from a signal handler to let the Forth environment know a signal
has been caught. It will then set a register that can (but not necessarily 
will be) checked when the Forth environment runs again.

@param  o   initialized forth environment
@param  sig caught signal value
**/
void forth_signal(forth_t *o, int sig);

/**
@brief  Duplicate a string, not all C libraries have a strdup function,
although they should!
@param  s String to duplicate
@return Duplicated string, caller frees.
**/
char *forth_strdup(const char *s);

/**
@brief Free the list of words returned by forth_words
@param s      word list to free
@param length length of word list to free
**/
void forth_free_words(char **s, size_t length);

/**
@brief This function returns a list of strings containing
all of the names of words defined in a forth environment. This
function returns NULL on failure and sets length to zero.
@param o      initialized forth environment
@param length length of returned array
@return list of pointers to strings containing Forth word names
**/
char **forth_words(forth_t *o, size_t *length);

/**
@brief  Check whether a forth environment is still valid, that
is if the environment makes sense and is still runnable, an invalid
forth environment cannot be saved to disk or run. Once a core is
invalidated it cannot be made valid again.
@param   o initialized forth environment
@return  zero if environment is still valid, non zero if it is not
**/
int forth_is_invalid(forth_t *o);

/**
@brief Invalidate a Forth environment.
@param o initialized forth environment to invalidate.
**/
void forth_invalidate(forth_t *o);

/**
@brief Set the verbosity/log/debug level of the interpreter, higher
values mean more verbose output.
@param o initialized forth environment.
@param level to set environment to.
**/
void forth_set_debug_level(forth_t *o, enum forth_debug_level level);

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
passed to forth_eval() after you have called forth_eval(). 
Multiple calls to forth_eval()
will work however.

@param   o   An initialized forth environment. Caller frees.
@param   s   A NUL terminated string to read from.
@return  int This is an error code, less than one is an error. 
**/
int forth_eval(forth_t *o, const char *s); 

/** 
@brief  Dump a raw forth object to disk, for debugging purposes, this
cannot be loaded with "forth_load_core_file".

@param  o    forth object to dump, caller frees, asserted.
@param  dump file to dump to (opened as "wb"), caller frees, asserted.
@return int 0 if successful, non zero otherwise 
**/
int forth_dump_core(forth_t *o, FILE *dump);

/** 
@brief   Save the opaque FORTH object to file, this file may be
loaded again with forth_load_core_file. The file passed in should
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
int forth_save_core_file(forth_t *o, FILE *dump);

/** 
@brief  Load a Forth file from disk, returning a forth object that
can be passed to forth_run. The loaded core file will have it's
input and output file-handles defaulted so it reads from standard
in and writes to standard error.

@param  dump    a file handle opened on a Forth core dump, previously
saved with forth_save_core, this must be opened
in binary mode ("rb").
@return forth_t a reinitialized forth object, or NULL on failure
**/
forth_t *forth_load_core_file(FILE *dump);

/**
@brief Load a core file from memory, much like forth_load_core_file. The
size parameter must be greater or equal to the MINIMUM_CORE_SIZE, this
is asserted. 

@param m    memory containing a Forth core file
@param size size of core file in memory in bytes
@return forth_t a reinitialized forth object, or NULL on failure
**/
forth_t *forth_load_core_memory(char *m, size_t size);

/**
@brief Save a Forth object to memory, this function will allocate
enough memory to store the core file. 

@param o    forth object to save to memory, Asserted.
@param[out] size of returned object, in bytes
@return pointer to saved memory on success.
**/
char *forth_save_core_memory(forth_t *o, size_t *size);

/** 
@brief   Define a new constant in an Forth environment.

@param   o    Forth environment to define new constant in
@param   name Name of constant, should be less than 31 characters in
length as only they will be used in defining the new
name
@param   c    Value of constant
@return  Same return status as forth_eval 
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
@brief A wrapper around fopen, exposed as a utility function, this
function either succeeds or calls "exit(EXIT_FAILURE)" after printing
an error message.
@param  name of file to open
@param  mode to open file in
@return always returns a file handle
**/
FILE *forth_fopen_or_die(const char *name, char *mode);

/**
@brief This is a simple wrapper around strerror, if the errno is
zero it returns "unknown error", or if strerror returns NULL. This function
inherits the problems of strerror (it is not threadsafe).
@return error string.
**/
const char *forth_strerror(void);

/** 
@brief  This implements a limited FORTH REPL.

Currently there is no mechanism for passing a struct forth_functions
to this call, this is deliberate. A saved Forth file will not make
sense without the correct forth_functions structure and associated
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

