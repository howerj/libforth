/** @file     unit.c
 *  @brief    unit tests for libforth interpreter public interface
 *  @author   Richard Howe
 *  @license  MIT (see https://opensource.org/licenses/MIT)
 *  @email    howe.r.j.89@gmail.com 
 *  @note     This file could be built into the main program, so that a series
 *            of built in tests can always be run. **/

/*** module to test ***/
#include "libforth.h"
/**********************/

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*** very minimal test framework ***/

typedef struct {
	unsigned passed, failed; /** number of tests passed and failed */
	double timer;
	clock_t start_time, end_time;
	time_t rawtime;
	int color_on;      /**< Is colorized output on?*/
	int jmpbuf_active; /**< Have we setup the longjmp buffer or not? */
	int is_silent;     /**< Silent mode on? The programs return code is used to determine success*/
	jmp_buf current_test; /**< current unit tests jump buffer */
	unsigned current_line; /**< current line number of unit test being executed */
	int current_result;    /**< result of latest test execution */
	const char *current_expr;  /**< string representation of expression being executed */
	FILE *output;
	int caught_signal; /**@warning signal catching is supported, but not thread safe */
} test_t;

static test_t tb;

#define MAX_SIGNALS (256) /**< maximum number of signals to decode */
static char *(sig_lookup[]) = { /*List of C89 signals and their names*/
	[SIGABRT]       = "SIGABRT",
	[SIGFPE]        = "SIGFPE",
	[SIGILL]        = "SIGILL",
	[SIGINT]        = "SIGINT",
	[SIGSEGV]       = "SIGSEGV",
	[SIGTERM]       = "SIGTERM",
	[MAX_SIGNALS]   = NULL
};

static void print_caught_signal_name(test_t *t) 
{
	char *sig_name = "UNKNOWN SIGNAL";
	if((t->caught_signal > 0) && (t->caught_signal < MAX_SIGNALS) && sig_lookup[t->caught_signal])
		sig_name = sig_lookup[t->caught_signal];
	if(!(t->is_silent))
		fprintf(t->output, "caught %s (signal number %d)\n", sig_name, t->caught_signal);\
}

/**@warning not thread-safe, this function uses internal static state*/
static void sig_abrt_handler(int sig) 
{ /* catches assert() from within functions being exercised */
	tb.caught_signal = sig;
	if(tb.jmpbuf_active) {
		tb.jmpbuf_active = 0;
		longjmp(tb.current_test, 1);
	}
}

static const char *reset(test_t *t)  { return t->color_on ? "\x1b[0m"  : ""; }
static const char *red(test_t *t)    { return t->color_on ? "\x1b[31m" : ""; }
static const char *green(test_t *t)  { return t->color_on ? "\x1b[32m" : ""; }
static const char *yellow(test_t *t) { return t->color_on ? "\x1b[33m" : ""; }
static const char *blue(test_t *t)   { return t->color_on ? "\x1b[34m" : ""; }

static int unit_tester(test_t *t, const int test, const char *msg, unsigned line) 
{
	assert(t && msg);
	if(test) {
	       	t->passed++; 
		if(!(t->is_silent))
			fprintf(t->output, "      %sok%s:\t%s\n", green(t), reset(t), msg); 
	} else { 
		t->failed++;
	       	if(!(t->is_silent))
			fprintf(t->output, "  %sFAILED%s:\t%s (line %d)\n", red(t), reset(t), msg, line);
	}
	return test;
}

static void print_statement(test_t *t, const char *stmt) 
{
	assert(t);
	if(!(t->is_silent))
		fprintf(t->output, "   %sstate%s:\t%s\n", blue(t), reset(t), stmt);
}

static void print_must(test_t *t, const char *must) 
{
	assert(t);
	if(!(t->is_silent))
		fprintf(t->output, "    %smust%s:\t%s\n", blue(t), reset(t), must);
}

static void print_note(test_t *t, const char *name) 
{ 
	assert(t);
	if(!(t->is_silent))
		fprintf(t->output, "%s%s%s\n", yellow(t), name, reset(t)); 
}

/**@brief Advance the test suite by testing and executing an expression. This
 *        framework can catch assertions that have failed within the expression
 *        being tested.
 * @param EXPR The expression should yield non zero on success **/
#define test(TESTBENCH, EXPR) _test((TESTBENCH), (EXPR) != 0, #EXPR, __LINE__)

static void _test(test_t *t, const int result, const char *expr, const unsigned line)
{
	assert(t && expr);
	t->current_line = line,
	t->current_expr = expr;
	signal(SIGABRT, sig_abrt_handler);
	if(!setjmp(t->current_test)) {
		t->jmpbuf_active = 1;
		t->current_result = unit_tester(t, result, t->current_expr, t->current_line);
	} else {
		print_caught_signal_name(t);
		t->current_result = unit_tester(t, 0, t->current_expr, t->current_line);
		signal(SIGABRT, sig_abrt_handler);
	}
	signal(SIGABRT, SIG_DFL);
	t->jmpbuf_active = 0;
}

/**@brief This advances the test suite like the test macro, however this test
 * must be executed otherwise the test suite will not continue
 * @param EXPR The expression should yield non zero on success */
#define must(TESTBENCH, EXPR) _must((TESTBENCH), (EXPR) != 0, #EXPR, __LINE__)

static void _must(test_t *t, const int result, const char *expr, const unsigned line)
{
	assert(t && expr);
	print_must(t, expr);
	_test(t, result, expr, line);
	if(!(t->current_result))
		exit(-1);
}

/**@brief print out and execute a statement that is needed to further a test
 * @param STMT A statement to print out (stringify first) and then execute**/
#define state(TESTBENCH, STMT) do{ print_statement((TESTBENCH), #STMT ); STMT; } while(0);

static int unit_test_start(test_t *t, const char *unit_name, FILE *output) 
{
	assert(t && unit_name && output);
	time(&t->rawtime);
	t->output = output;
	if(signal(SIGABRT, sig_abrt_handler) == SIG_ERR) {
		fprintf(stderr, "signal handler installation failed");
		return -1;
	}
	t->start_time = clock();
	if(!(t->is_silent))
		fprintf(t->output, "%s unit tests\n%sbegin:\n\n", 
				unit_name, asctime(localtime(&(t->rawtime))));
	return 0;
}

static unsigned unit_test_end(test_t *t, const char *unit_name) 
{
	assert(t && unit_name);
	t->end_time = clock();
	t->timer = ((double) (t->end_time - t->start_time)) / CLOCKS_PER_SEC;
	if(!(t->is_silent))
		fprintf(t->output, "\n\n%s unit tests\npassed  %u/%u\ntime    %fs\n", 
				unit_name, t->passed, t->passed+t->failed, t->timer);
	return t->failed;
}

/*** end minimal test framework ***/

static char usage[] = "\
libforth unit test framework\n\
\n\
	usage: %s [-h] [-c] [-k] [-s] [-]\n\
\n\
	-h	print this help message and exit (unsuccessfully so tests do not pass)\n\
	-c	turn colorized output on (forced on)\n\
	-k	keep any temporary file\n\
	-s	silent mode\n\
	-       stop processing command line arguments\n\
\n\
This program executes are series of tests to exercise the libforth library. It\n\
will return zero on success and non zero on failure. The tests and results will\n\
be printed out as executed.\n\
\n";

static int keep_files = 0;
int main(int argc, char **argv) {
	int i;
	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		switch(argv[i][1]) {
			case '\0':
				goto done;
			case 's':
				tb.is_silent = 1;
				break;
			case 'h': 
				fprintf(stderr, usage, argv[0]);
				return -1;
			case 'c': 
				tb.color_on = 1;
				break;
			case 'k': 
				keep_files = 1; 
				break;
			default:
				fprintf(stderr, "invalid argument '%s'\n", argv[i]);
				fprintf(stderr, usage, argv[0]);
				return -1;
		}
done:
	unit_test_start(&tb, "libforth", stdout);
	{
		/**@note The following functions will not be tested:
		 * 	- void forth_set_file_output(forth_t *o, FILE *out);
		 * 	- void forth_set_args(forth_t *o, int argc, char **argv);
		 *	- int main_forth(int argc, char **argv); **/
		FILE *core;
		forth_cell_t here;
		forth_t *f;
		print_note(&tb, "libforth.c");
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(&tb, f);
		state(&tb, core = fopen("unit.core", "wb"));
		must(&tb, core);

		/* test setup, simple tests of push/pop interface */
		test(&tb, 0 == forth_stack_position(f));
		test(&tb, forth_eval(f, "here ")  >= 0);
		state(&tb, here = forth_pop(f));
		state(&tb, forth_push(f, here));
		test(&tb, forth_eval(f, "2 2 + ") >= 0);
		test(&tb, forth_pop(f) == 4);
		/* define a word, call that word, pop result */
		test(&tb, !forth_find(f, "unit-01"));
		test(&tb, forth_eval(f, ": unit-01 69 ; unit-01 ") >= 0);
		test(&tb, forth_find(f,  "unit-01"));
		test(&tb, !forth_find(f, "unit-01 ")); /* notice the trailing space */
		test(&tb, forth_pop(f) == 69);
		test(&tb, 1 == forth_stack_position(f)); /* "here" still on stack */

		/* constants */
		test(&tb, forth_define_constant(f, "constant-1", 0xAA0A) >= 0);
		test(&tb, forth_define_constant(f, "constant-2", 0x5055) >= 0);
		test(&tb, forth_eval(f, "constant-1 constant-2 or") >= 0);
		test(&tb, forth_pop(f) == 0xFA5F);

		/* string input */
		state(&tb, forth_set_string_input(f, " 18 2 /"));
		test(&tb, forth_run(f) >= 0);
		test(&tb, forth_pop(f) == 9);
		state(&tb, forth_set_file_input(f, stdin));

		/* save core for later tests */
		test(&tb, forth_save_core(f, core) >= 0);
		state(&tb, fclose(core));

		/* more simple tests of arithmetic */
		state(&tb, forth_push(f, 99));
		state(&tb, forth_push(f, 98));
		test(&tb, forth_eval(f, "+") >= 0);
		test(&tb, forth_pop(f) == 197);
		test(&tb, 1 == forth_stack_position(f)); /* "here" still on stack */
		test(&tb, here == forth_pop(f));
		state(&tb, forth_free(f));
	}
	{
		FILE *core_dump;
		forth_t *f = NULL;
		state(&tb, core_dump = tmpfile());
		must(&tb, core_dump);
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(&tb, f);
		test(&tb, forth_dump_core(f, core_dump) >= 0);
		state(&tb, fclose(core_dump));
		state(&tb, forth_free(f));
	}
	{
		/* Test the persistence of word definitions across core loads*/
		FILE *core;
		forth_t *f;
		state(&tb, core = fopen("unit.core", "rb"));
		must(&tb, core);

		/* test that definitions persist across core dumps */
		state(&tb, f = forth_load_core(core));
		/* stack position does no persist across loads, this might
		 * change, but is the current functionality */
		test(&tb, 0 == forth_stack_position(f)); 
		must(&tb, f);
		/* the word "unit-01" was defined earlier */
		test(&tb, forth_find(f, "unit-01"));
		test(&tb, forth_eval(f, "unit-01 constant-1 *") >= 0);
		test(&tb, forth_pop(f) == 69 * 0xAA0A);
		test(&tb, 0 == forth_stack_position(f));

		state(&tb, forth_free(f));
		state(&tb, fclose(core));
		if(!keep_files)
			state(&tb, remove("unit.core"));
	}
	{
		/* test the built in words, there is a set of built in words
		 * that are defined in the interpreter, these must be tested 
		 *
		 * The following words need testing:
		 * 	[ ] :noname 
		 * 	'\n' ')' cr :: 
		 */
		forth_t *f;
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(&tb, f);

		/* here we test if...else...then statements and hex conversion,
		 * this also tests >mark indirectly */
		test(&tb, forth_eval(f, ": if-test if 0x55 else 0xAA then ;") >= 0);
		test(&tb, forth_eval(f, "0 if-test") >= 0);
		test(&tb, forth_pop(f) == 0xAA);
		state(&tb, forth_push(f, 1));
		test(&tb, forth_eval(f, "if-test") >= 0);
		test(&tb, forth_pop(f) == 0x55);

		/* simple loop tests */
		test(&tb, forth_eval(f, " : loop-test begin 1 + dup 10 u> until ;") >= 0);
		test(&tb, forth_eval(f, " 1 loop-test") >= 0);
		test(&tb, forth_pop(f) == 11);
		test(&tb, forth_eval(f, " 39 loop-test") >= 0);
		test(&tb, forth_pop(f) == 40);

		/* rot and comments */
		test(&tb, forth_eval(f, " 1 2 3 rot ( 1 2 3 -- 2 3 1 )") >= 0);
		test(&tb, forth_pop(f) == 1);
		test(&tb, forth_pop(f) == 3);
		test(&tb, forth_pop(f) == 2);

		/* -rot */
		test(&tb, forth_eval(f, " 1 2 3 -rot ") >= 0);
		test(&tb, forth_pop(f) == 2);
		test(&tb, forth_pop(f) == 1);
		test(&tb, forth_pop(f) == 3);

		/* nip */
		test(&tb, forth_eval(f, " 3 4 5 nip ") >= 0);
		test(&tb, forth_pop(f) == 5);
		test(&tb, forth_pop(f) == 3);

		/* allot */
		test(&tb, forth_eval(f, " here 32 allot here swap - ") >= 0);
		test(&tb, forth_pop(f) == 32);

		/* tuck */
		test(&tb, forth_eval(f, " 67 23 tuck ") >= 0);
		test(&tb, forth_pop(f) == 23);
		test(&tb, forth_pop(f) == 67);
		test(&tb, forth_pop(f) == 23);

		state(&tb, forth_free(f));
	}
	{
		/* test the forth interpreter internals */
		forth_t *f;
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(&tb, f);

		/* base should be set to zero, this is a special value
		 * that allows hexadecimal, octal and decimal to be read
		 * in if formatted correctly;
		 * 	- hex     0x[0-9a-fA-F]*
		 * 	- octal   0[0-7]*
		 * 	- decimal [1-9][0-9]* 
		 */
		test(&tb, forth_eval(f, " base @ 0 = ") >= 0);
		test(&tb, forth_pop(f));

		/* the invalid flag should not be set */
		test(&tb, forth_eval(f, " `invalid @ 0 = ") >= 0);
		test(&tb, forth_pop(f));

		/* source id should be -1 (reading from string) */
		test(&tb, forth_eval(f, " `source-id @ -1 = ") >= 0);
		test(&tb, forth_pop(f));

		state(&tb, forth_free(f));
	}
	return !!unit_test_end(&tb, "libforth");
}

