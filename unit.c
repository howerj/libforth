/** 
@file     unit.c
@brief    unit tests for libforth interpreter public interface
@author   Richard Howe
@license  MIT (see https://opensource.org/licenses/MIT)
@email    howe.r.j.89@gmail.com 
**/  

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

/**
@brief This contains the information needed to complete a series of unit
tests, along with various options.
**/
typedef struct {
	unsigned passed, /**< number of unit tests passed */
		 failed; /**< number of unit tests failed */
	clock_t start_time; /**< when the unit tests began */
	int color_on;      /**< is colorized output on?*/
	int jmpbuf_active; /**< have we setup the longjmp buffer or not? */
	int is_silent;     /**< silent mode on? The programs return code is used to determine success*/
	jmp_buf current_test; /**< current unit tests jump buffer */
	unsigned current_line; /**< current line number of unit test being executed */
	int current_result;    /**< result of latest test execution */
	const char *current_expr;  /**< string representation of expression being executed */
	FILE *output; /**< output file for report generation */
	/**@warning signal catching is supported, but not thread safe. Signal
	 * handling really should be made optional, but there is no need for
	 * such a small test suite. */
	int caught_signal; /**< The value of any caught signals when running tests*/
} test_t /**< structure used to hold test information */;

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

/*
static size_t compare(const char *restrict a, const char *restrict b, size_t c)
{
	for (size_t i = 0; i < c; i++)
		if (a[i] != b[i])
			return i;
	return 0;
}*/

static void print_caught_signal_name(test_t *t) 
{
	char *sig_name = "UNKNOWN SIGNAL";
	if ((t->caught_signal > 0) && (t->caught_signal < MAX_SIGNALS) && sig_lookup[t->caught_signal])
		sig_name = sig_lookup[t->caught_signal];
	if (!(t->is_silent))
		fprintf(t->output, "caught %s (signal number %d)\n", sig_name, t->caught_signal);\
}

/**@warning not thread-safe, this function uses internal static state*/
static void sig_abrt_handler(int sig) 
{ /* catches assert() from within functions being exercised */
	tb.caught_signal = sig;
	if (tb.jmpbuf_active) {
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
	if (test) {
	       	t->passed++; 
		if (!(t->is_silent))
			fprintf(t->output, "      %sok%s:\t%s\n", green(t), reset(t), msg); 
	} else { 
		t->failed++;
	       	if (!(t->is_silent))
			fprintf(t->output, "  %sFAILED%s:\t%s (line %u)\n", red(t), reset(t), msg, line);
	}
	return test;
}

static void print_statement(test_t *t, const char *stmt) 
{
	assert(t);
	if (!(t->is_silent))
		fprintf(t->output, "   %sstate%s:\t%s\n", blue(t), reset(t), stmt);
}

static void print_must(test_t *t, const char *must) 
{
	assert(t);
	if (!(t->is_silent))
		fprintf(t->output, "    %smust%s:\t%s\n", blue(t), reset(t), must);
}

static void print_note(test_t *t, const char *name) 
{ 
	assert(t);
	if (!(t->is_silent))
		fprintf(t->output, "%s%s%s\n", yellow(t), name, reset(t)); 
}

/**@brief Advance the test suite by testing and executing an expression. This
 *        framework can catch assertions that have failed within the expression
 *        being tested.
 * @param TESTBENCH The test bence to execute under
 * @param EXPR The expression should yield non zero on success **/
#define test(TESTBENCH, EXPR) _test((TESTBENCH), (EXPR) != 0, #EXPR, __LINE__)

static void _test(test_t *t, const int result, const char *expr, const unsigned line)
{
	assert(t && expr);
	t->current_line = line,
	t->current_expr = expr;
	signal(SIGABRT, sig_abrt_handler);
	if (!setjmp(t->current_test)) {
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
 * @param TESTBENCH The test bence to execute under
 * @param EXPR The expression should yield non zero on success */
#define must(TESTBENCH, EXPR) _must((TESTBENCH), (EXPR) != 0, #EXPR, __LINE__)

static void _must(test_t *t, const int result, const char *expr, const unsigned line)
{
	assert(t && expr);
	print_must(t, expr);
	_test(t, result, expr, line);
	if (!(t->current_result))
		exit(-1);
}

/**@brief print out and execute a statement that is needed to further a test
 * @param TESTBENCH The test bence to execute under
 * @param STMT A statement to print out (stringify first) and then execute**/
#define state(TESTBENCH, STMT) do{ print_statement((TESTBENCH), #STMT ); STMT; } while (0);

static int unit_test_start(test_t *t, const char *unit_name, FILE *output) 
{
	assert(t && unit_name && output);
	memset(t, 0, sizeof(*t));
	time_t rawtime;
	time(&rawtime);
	t->output = output;
	if (signal(SIGABRT, sig_abrt_handler) == SIG_ERR) {
		fprintf(stderr, "signal handler installation failed");
		return -1;
	}
	t->start_time = clock();
	if (!(t->is_silent))
		fprintf(t->output, "%s unit tests\n%sbegin:\n\n", 
				unit_name, asctime(localtime(&rawtime)));
	return 0;
}

static unsigned unit_test_end(test_t *t, const char *unit_name) 
{
	assert(t && unit_name);
	clock_t end_time = clock();
	double total = ((double) (end_time - t->start_time)) / CLOCKS_PER_SEC;
	if (!(t->is_silent))
		fprintf(t->output, "\n\n%s unit tests\npassed  %u/%u\ntime    %fs\n", 
				unit_name, t->passed, t->passed+t->failed, total);
	return t->failed;
}

/*** end minimal test framework ***/

/* forth_function_1 and forth_function_2 are 
test functions that can be called from within the interpreter */
static int forth_function_1(forth_t *f)
{
	forth_push(f, 123);
	return 0;
}

static int forth_function_2(forth_t *f)
{
	forth_push(f, 789);
	return 0;
}

int libforth_unit_tests(int keep_files, int colorize, int silent)
{
	tb.is_silent = silent;
	tb.color_on  = colorize;

	unit_test_start(&tb, "libforth", stdout);
	{
		test(&tb, 0 == forth_blog2(0));
		test(&tb, 0 == forth_blog2(1));
		test(&tb, 1 == forth_blog2(2));
		test(&tb, 2 == forth_blog2(4));
		test(&tb, 3 == forth_blog2(8));
		test(&tb, 3 == forth_blog2(10));
		test(&tb, 4 == forth_blog2(16));
		test(&tb, 4 == forth_blog2(17));
	
		test(&tb, 1  == forth_round_up_pow2(0));
		test(&tb, 1  == forth_round_up_pow2(1));
		test(&tb, 2  == forth_round_up_pow2(2));
		test(&tb, 4  == forth_round_up_pow2(3));
		test(&tb, 16 == forth_round_up_pow2(9));
		test(&tb, 64 == forth_round_up_pow2(37));
	}
	{
		/**@note The following functions will not be tested:
		 * 	- void forth_set_file_output(forth_t *o, FILE *out);
		 * 	- void forth_set_args(forth_t *o, int argc, char **argv);
		 * 	- void forth_signal(forth_t *o, int signal);
		 *	- int main_forth(int argc, char **argv); **/
		FILE *core;
		forth_cell_t here;
		forth_t *f;
		print_note(&tb, "libforth.c");
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout, NULL));
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
		test(&tb, forth_save_core_file(f, core) >= 0);
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
		/**@note Previously 'tmpfile()' was used instead of writing to
		 * 'coredump.log', however this causes problems under Windows 
		 * as it creates the temporary file in the root directory, 
		 * which needs administrator permissions.
		 *
		 * https://stackoverflow.com/questions/6247148/tmpfile-on-windows-7-x64 */
		FILE *core_dump;
		forth_t *f = NULL;
		static const char *name = "coredump.log";
		state(&tb, core_dump = fopen(name, "wb"));
		must(&tb, core_dump);
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout, NULL));
		must(&tb, f);
		test(&tb, forth_dump_core(f, core_dump) >= 0);
		state(&tb, fclose(core_dump));
		state(&tb, forth_free(f));
		if (!keep_files)
			state(&tb, remove(name));
	}
	{
		/* Test the persistence of word definitions across core loads*/
		FILE *core;
		forth_t *f;
		state(&tb, core = fopen("unit.core", "rb"));
		must(&tb, core);

		/* test that definitions persist across core dumps */
		state(&tb, f = forth_load_core_file(core));
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
	}
	{ /* test invalidation fails */
		FILE *core;
		forth_t *f;
		state(&tb, core = fopen("unit.core", "rb+"));
		must(&tb, core);
		state(&tb, rewind(core));

		state(&tb, f = forth_load_core_file(core));
		test(&tb, !forth_is_invalid(f));
		state(&tb, forth_invalidate(f));
		test(&tb,  forth_is_invalid(f));
		/* saving should fail as we have invalidated the core */
		test(&tb, forth_save_core_file(f, core) < 0);
		state(&tb, forth_free(f));
		state(&tb, fclose(core));
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
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout, NULL));
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
		forth_t *f = NULL;
		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout, NULL));
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

		/* 0 call should fail, returning non zero */
		test(&tb, forth_eval(f, "0 call") >= 0);
		test(&tb, forth_pop(f)); 

		state(&tb, forth_free(f));
	}
	{ /* tests for CALL */
		forth_t *f = NULL;
		struct forth_functions *ff;
		state(&tb, ff = forth_new_function_list(2));
		must(&tb, ff);

		state(&tb, ff->functions[0].function = forth_function_1);
		state(&tb, ff->functions[1].function = forth_function_2);

		state(&tb, f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout, ff));
		must(&tb, f);

		/* 0 call should correspond to the first function, which just
		 * pushes 123 */
		test(&tb, forth_eval(f, "0 call") >= 0);
		test(&tb, !forth_pop(f));
		test(&tb, 123 == forth_pop(f));

		/* 1 call corresponds to the second function... */
		test(&tb, forth_eval(f, "1 call") >= 0);
		test(&tb, !forth_pop(f));
		test(&tb, 789 == forth_pop(f));

		test(&tb, forth_eval(f, "2 call") >= 0);
		/* call signals failure by returning non zero */
		test(&tb, forth_pop(f)); 

		state(&tb, forth_free(f));
		state(&tb, forth_delete_function_list(ff));
	}
	{ 
		FILE *core = NULL;
		forth_t *f1 = NULL, *f2 = NULL;
		char *m1 = NULL, *m2 = NULL;
		forth_cell_t size1, size2;
		must(&tb, f1 = forth_init(MINIMUM_CORE_SIZE, stdin, stdout, NULL));
		state(&tb, core = fopen("unit.core", "wb+"));
		must(&tb, core);
		test(&tb, forth_save_core_file(f1, core) >= 0);
		state(&tb, rewind(core));

		must(&tb, m1 = forth_save_core_memory(f1, &size1));
		must(&tb, f2 = forth_load_core_memory(m1,  size1));
		must(&tb, m2 = forth_save_core_memory(f2, &size2));
		must(&tb, size2 == size1);
		test(&tb, size1/sizeof(forth_cell_t) > MINIMUM_CORE_SIZE);

		state(&tb, fclose(core));
		state(&tb, forth_free(f1));
		state(&tb, forth_free(f2));
		state(&tb, free(m1));
		state(&tb, free(m2));

		if (!keep_files)
			state(&tb, remove("unit.core"));
	}
	return !!unit_test_end(&tb, "libforth");
}

