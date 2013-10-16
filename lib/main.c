/** Howe Forth
 *
 * @file main.c
 * @brief Wrapper/Top Level.
 *
 * @author         Richard James Howe.
 * @copyright      Copyright 2013 Richard James Howe.
 * @license        LGPL      
 * @email          howe.r.j.89@gmail.com
 *
 */

#include <stdio.h>              /** required by hosted.h and forth.h */
#include <stdlib.h>             /** required by hosted.h */
#include <string.h>             /** strcmp */
#include <stdint.h>             /** required by forth.h */
#include <signal.h>             /** required by main.c, for handling signals...*/
#include "forth.h"              /** forth_monitor, fobj_t */
#include "hosted.h"             /** forth_obj_create, forth_obj_destroy */

/*****************************************************************************/
/*** Macros; parameters ******************************************************/
/*****************************************************************************/

#define MAX_REG (32)
#define MAX_DIC (1024*1024)
#define MAX_VAR (8192)
#define MAX_RET (8192)
#define MAX_STR (1024*1024)

/*****************************************************************************/
/*** Macros; Function like ***************************************************/
/*****************************************************************************/

#define SET_SIGNAL(SIGNAL,HANDLER) if(signal((SIGNAL),(HANDLER)) == SIG_ERR){\
    fprintf(stderr,#SIGNAL ":" #HANDLER " failed <%s:%d>\n", __FILE__,__LINE__);\
    return 1;\
  }

/*****************************************************************************/
/*** Function prototypes *****************************************************/
/*****************************************************************************/

static void print_enums(FILE * output);
static void handle_signals(int signal);

/*****************************************************************************/
/*** Strings, constant *******************************************************/
/*****************************************************************************/

/** versionmsg, used for a primitive version control */
static const char versionmsg[] = "Howe Forth, version: " __DATE__ " " __TIME__ "\n";

/** The help message! */
static const char helpmsg[] = "\
Usage: forth [OPTION/FILE]\n\
Run the Howe Forth interpreter.\n\
  -h, --help      Display this help message and exit.\n\
  -V, --version   Display the version number and exit.\n\
  -E, --enum      Print out a list of all enums and exit.\n\
\n\
With no OPTION or FILE given Howe Forth will attempt to read the\n\
input file \"forth.4th\", otherwise if a valid file name is given\n\
that will be read in. Once it has it finished it will continue\n\
reading from stdin.\n\
";

static const char bugmsg[] = "\
The program should not be in this state, please file a bug report to:\n\
    <howe.r.j.89@gmail.com>\n\
Calling \"exit(1)\"...";

/*****************************************************************************/
/*** X-macro magic ***********************************************************/
/*****************************************************************************/

/**only for printing out*/
#define X(a, b) b
static const char *forth_primitives_str_print_me[] = {
  FORTH_PRIMITIVE_XMACRO_M
};
#undef X

/**only for printing out*/
#define X(a, b) b
static const char *forth_system_calls_str_print_me[] = {
  FORTH_SYSTEM_CALLS_XMACRO_M
};
#undef X

/**only for printing out*/
#define X(a, b) b
static const char *forth_register_str_print_me[] = {
  FORTH_REGISTER_ENUM_XMACRO_M
};
#undef X

/*****************************************************************************/
/*** Function definitions ****************************************************/
/*****************************************************************************/

/**Print out all of the defined enumerations that might be
 * useful for the interpreter when it is running to have*/
static void print_enums(FILE * output)
{
  int i;

  fprintf(output, "\\ Auto generated file, do not edit \n");
  fprintf(output, "\\ This file contains a list of useful constants\n\n");

  /**print out registers*/
  fprintf(output, "\\ register locations\n");
  for (i = 0; i < REG_LAST_REGISTER; i++) {
    fprintf(output, "%d constant R_%s\n", i, forth_register_str_print_me[i]);
  }

  /**print out system calls*/
  fprintf(output, "\\ system calls for kernel keyword\n");
  for (i = 0; i < SYS_LAST_ERROR_CALL; i++) {
    fprintf(output, "%d constant S_%s\n", i, forth_system_calls_str_print_me[i]);
  }

  /**print out defined primitives*/
  fprintf(output, "\\ primitives\n");
  for (i = 0; i < P_LAST_PRIMITIVE; i++) {
    fprintf(output, "%d constant P_%s\n", i, forth_primitives_str_print_me[i]);
  }

  return;
}

/** handle all standard c signals, I should not be
 * using printf here, but I will until I find a better
 * way that is *also* portable  */
static void handle_signals(int signal){
  switch(signal){
    case SIGABRT: /** abort!*/
      break;
    case SIGSEGV: /** bug*/
      fprintf(stderr,"SIGSEGV:%s:%d:\n%s\n", __FILE__, __LINE__, bugmsg);
      exit(1);
    case SIGTERM: /** clean up, exit */
      exit(1);
    case SIGFPE:  /** bug*/
      fprintf(stderr,"SIGFPE:%s:%d:\n%s\n", __FILE__, __LINE__, bugmsg);
      exit(1);
    case SIGILL:  /** bug*/
      fprintf(stderr,"SIGILL:%s:%d:\n%s\n", __FILE__, __LINE__, bugmsg);
      exit(1);
    case SIGINT: /** ignore?*/
    default:
      break;
  }
}


/*****************************************************************************/
/*** main() ******************************************************************/
/*****************************************************************************/

/**main, where the magic happens, well is called from.*/
int main(int argc, char *argv[])
{
  FILE *input = NULL;
  mw forth_return;
  fobj_t *fo;

  if (argc > 1) {
                /**process command line arguments*/
    if ((!strcmp("-h", argv[1])) || (!strcmp("--help", argv[1]))) {
      fprintf(stdout, "%s using: %s\n%s\n", versionmsg, WORD_STRING, helpmsg);
      return 0;
    }

    if ((!strcmp("-V", argv[1])) || (!strcmp("--version", argv[1]))) {
      fprintf(stdout, "%s using: %s\n", versionmsg, WORD_STRING);
      return 0;
    }

    if ((!strcmp("-E", argv[1])) || (!strcmp("--enum", argv[1]))) {
      print_enums(stdout);
      return 0;
    }
    /*
       if((!strcmp("-",argv[1]))||(!strcmp("--stdin",argv[1]))){
       } */
    if (NULL == (input = fopen(argv[1], "r"))) {
      fprintf(stderr, "Could not open \"%s\" for reading.\n", argv[1]);
      return 1;
    }
  }
  fo = forth_obj_create(MAX_REG, MAX_DIC, MAX_VAR, MAX_RET, MAX_STR, input);
  CALLOC_FAIL(fo, -1);          /*memory might not be free()'d on error. */

  /** register signal handlers */

  SET_SIGNAL(SIGABRT,handle_signals);
  SET_SIGNAL(SIGSEGV,handle_signals);
  SET_SIGNAL(SIGTERM,handle_signals);
  SET_SIGNAL(SIGFPE,handle_signals);
  SET_SIGNAL(SIGILL,handle_signals);
  SET_SIGNAL(SIGINT,handle_signals);

  /** call the forth monitor, which monitors the forth virtual machine for
   * errors and handles them */
  forth_return = forth_monitor(fo);

  fprintf(stderr, "(returned %X)\n", (unsigned int)forth_return);
  forth_obj_destroy(fo);
  return 0;
}

/*****************************************************************************/
/*** EOF *********************************************************************/
/*****************************************************************************/
