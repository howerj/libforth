/**
@todo cleanup comments / explain two stage compilation
@todo add back in line editor option
@todo add signal handling here
**/
#include "libforth.h"
#include "unit.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#ifdef USE_BUILT_IN_CORE
extern unsigned char forth_core_data[];
extern forth_cell_t forth_core_size;
#endif

/** 
This program can be used as a filter in a Unix pipe chain, or as a standalone
interpreter for Forth. It tries to follow the Unix philosophy and way of
doing things (see <http://www.catb.org/esr/writings/taoup/html/ch01s06.html>
and <https://en.wikipedia.org/wiki/Unix_philosophy>). Whether this is
achieved is a matter of opinion. There are a things this interpreter does
differently to most Forth interpreters that support this philosophy however,
it is silent by default and does not clutter up the output window with "ok",
or by printing a banner at start up (which would contain no useful information
whatsoever). It is simple, and only does one thing (but does it do it well?).
**/
static void fclose_input(FILE **in)
{
	if(*in && (*in != stdin))
		fclose(*in);
	*in = stdin;
}

/**
It is customary for Unix programs to have a usage string, which we
can print out as a quick reminder to the user as to what the command
line options are.
**/
static void usage(const char *name)
{
	fprintf(stderr, 
		"usage: %s "
		"[-(s|l) file] [-e expr] [-m size] [-Vthv] [-] files\n", 
		name);
}

/** 
We try to keep the interface to the example program as simple as possible,
so there are limited, uncomplicated options. What they do
should come as no surprise to an experienced Unix programmer, it is important
to pick option names that they would expect (for example *-l* for loading,
*-e* for evaluation, and not using *-h* for help would be a hanging offense).
**/

static void help(void)
{
	static const char help_text[] =
"Forth: A small forth interpreter build around libforth\n\n"
"\t-h        print out this help and exit unsuccessfully\n"
"\t-u        run the built in unit tests, then exit\n"
"\t-e string evaluate a string\n"
"\t-s file   save state of forth interpreter to file\n"
"\t-d        save state to 'forth.core'\n"
"\t-l file   load previously saved state from file\n"
"\t-m size   specify forth memory size in KiB (cannot be used with '-l')\n"
"\t-t        process stdin after processing forth files\n"
"\t-v        turn verbose mode on\n"
"\t-V        print out version information and exit\n"
"\t-         stop processing options\n\n"
"Options must come before files to execute.\n\n"
"The following words are built into the interpreter:\n\n";

	fputs(help_text, stderr);

	/*for(unsigned i = 0; i < LAST_INSTRUCTION; i++)
		fprintf(stderr, "%s\t\t%s\n", 
				instruction_names[i],
				instruction_help_strings[i]);*/
}

static int eval_file(forth_t *o, const char *file, enum forth_debug_level verbose) {
	FILE *in = NULL;
	int c = 0, rval = 0;
	if(verbose >= FORTH_DEBUG_NOTE)
		note("reading from file '%s'", file);
	forth_set_file_input(o, in = forth_fopen_or_die(file, "rb"));
	/* shebang line '#!', core files could also be detected */
	if((c = fgetc(in)) == '#') 
		while(((c = fgetc(in)) > 0) && (c != '\n'));
	else if(c == EOF)
		goto close;
	else
		ungetc(c, in);
	rval = forth_run(o);
close:	
	fclose_input(&in);
	return rval;
}

static void version(void)
{
	fprintf(stdout, 
		"libforth:\n" 
		"\tversion:     %d\n"
		"\tsize:        %u\n"
		"\tendianess:   %u\n"
		/*"initial forth program:\n%s\n"*/,
		FORTH_CORE_VERSION, 
		(unsigned)sizeof(forth_cell_t) * CHAR_BIT, 
		(unsigned)IS_BIG_ENDIAN
		/*,initial_forth_program*/);
}

static forth_t *forth_initial_enviroment(forth_t **o, forth_cell_t size, 
		FILE *input, FILE *output, enum forth_debug_level verbose, 
		int argc, char **argv)
{
	errno = 0;
	assert(input && output);
	if(*o)
		goto finished;

#ifdef USE_BUILT_IN_CORE
	(void)size;
	*o = forth_load_core_memory((char*)forth_core_data, forth_core_size);
	forth_set_file_input(*o, input);
	forth_set_file_input(*o, output);
#else
	*o = forth_init(size, input, output, NULL);
#endif
	if(!(*o)) {
		fatal("forth initialization failed, %s", forth_strerror());
		exit(EXIT_FAILURE);
	}

finished:
	forth_set_debug_level(*o, verbose);
	forth_set_args(*o, argc, argv);
	return *o;
}

/**

To keep things simple options are parsed first then arguments like files,
although some options take arguments immediately after them. 

A library for parsing command line options like *getopt* should be used,
this would reduce the portability of the program. It is not recommended 
that arguments are parsed in this manner.
**/
int main(int argc, char **argv)
{
	FILE *in = NULL, *dump = NULL;
	int rval = 0, i = 1;
       	int save = 0,            /* attempt to save core if true */
	    eval = 0,            /* have we evaluated anything? */
	    readterm = 0,        /* read from standard in */
	    mset = 0;            /* memory size specified */
	enum forth_debug_level verbose = FORTH_DEBUG_OFF; /* verbosity level */
	static const size_t kbpc = 1024 / sizeof(forth_cell_t); /*kilobytes per cell*/
	static const char *dump_name = "forth.core";
	char *optarg = NULL;
	forth_cell_t core_size = DEFAULT_CORE_SIZE;
	forth_t *o = NULL;
	int orig_argc = argc;
	char **orig_argv = argv;

#ifdef _WIN32
	/* unmess up Windows file descriptors: there is a warning about an
	 * implicit declaration of _fileno when compiling under Windows in C99
	 * mode */
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);
#endif
/**
This loop processes any options that may have been passed to the program, it
looks for arguments beginning with '-' and attempts to process that option,
if the argument does not start with '-' the option processing stops. It is
a simple mechanism for processing program arguments and there are better
ways of doing it (such as "getopt" and "getopts"), but by using them we
sacrifice portability.
**/

	for(i = 1; i < argc && argv[i][0] == '-'; i++) {
		if(strlen(argv[i]) > 2) {
			fatal("Only one option allowed at a time (got %s)", argv[i]);
			goto fail;
		}
		switch(argv[i][1]) {
		case '\0': goto done; /* stop processing options */
		case 'h':  usage(argv[0]); 
			   help(); 
			   return -1;
		case 't':  readterm = 1; 
			   break;
		case 'u':
			   return libforth_unit_tests(0, 0, 0);
		case 'e':
			if(i >= (argc - 1))
				goto fail;
			forth_initial_enviroment(&o, core_size, stdin, stdout, verbose, orig_argc, orig_argv);
			optarg = argv[++i];
			if(verbose >= FORTH_DEBUG_NOTE)
				note("evaluating '%s'", optarg);
			if(forth_eval(o, optarg) < 0)
				goto end;
			eval = 1;
			break;
		case 'f':
			if(i >= (argc - 1))
				goto fail;
			forth_initial_enviroment(&o, core_size, stdin, stdout, verbose, orig_argc, orig_argv);
			optarg = argv[++i];
			if(verbose >= FORTH_DEBUG_NOTE)
				note("reading from file '%s'", optarg);
			if(eval_file(o, optarg, verbose) < 0)
				goto end;
			break;
		case 's':
			if(i >= (argc - 1))
				goto fail;
			dump_name = argv[++i];
			/* XXX fall through */
		case 'd':  /*use default name */
			if(verbose >= FORTH_DEBUG_NOTE)
				note("saving core file to '%s' (on exit)", dump_name);
			save = 1;
			break;
		case 'm':
			if(o || (i >= argc - 1) || forth_string_to_cell(10, &core_size, argv[++i]))
				goto fail;
			if((core_size *= kbpc) < MINIMUM_CORE_SIZE) {
				fatal("-m too small (minimum %zu)", MINIMUM_CORE_SIZE / kbpc);
				return -1;
			}
			if(verbose >= FORTH_DEBUG_NOTE)
				note("memory size set to %zu", core_size);
			mset = 1;
			break;
		case 'l':
			if(o || mset || (i >= argc - 1))
				goto fail;
			optarg = argv[++i];
			if(verbose >= FORTH_DEBUG_NOTE)
				note("loading core file '%s'", optarg);
			if(!(o = forth_load_core_file(dump = forth_fopen_or_die(optarg, "rb")))) {
				fatal("%s, core load failed", optarg);
				return -1;
			}
			forth_set_debug_level(o, verbose);
			fclose(dump);
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			version();
			return EXIT_SUCCESS;
			break;
		default:
		fail:
			fatal("invalid argument '%s'", argv[i]);
			usage(argv[0]);
			return -1;
		}
	}

done:
	/* if no files are given, read stdin */
	readterm = (!eval && i == argc) || readterm;
	forth_initial_enviroment(&o, core_size, stdin, stdout, verbose, orig_argc, orig_argv);

	for(; i < argc; i++) /* process all files on command line */
		if(eval_file(o, argv[i], verbose) < 0)
			goto end;

	if(readterm) { /* if '-t' or no files given, read from stdin */
		if(verbose >= FORTH_DEBUG_NOTE)
			note("reading from stdin (%p)", stdin);
		forth_set_file_input(o, stdin);
		rval = forth_run(o);
	}
end:	
	fclose_input(&in);

/**
If the save option has been given we only want to save valid core files,
we might want to make an option to force saving of core files for debugging
purposes, but in general we do not want to over write valid previously saved
state with invalid data.
**/

	if(save) { /* save core file */
		if(rval || forth_is_invalid(o)) {
			fatal("refusing to save invalid core, %u/%d", rval, forth_is_invalid(o));
			return -1;
		}
		if(verbose >= FORTH_DEBUG_NOTE)
			note("saving for file to '%s'", dump_name);
		if(forth_save_core_file(o, dump = forth_fopen_or_die(dump_name, "wb"))) {
			fatal("core file save to '%s' failed", dump_name);
			rval = -1;
		}
		fclose(dump);
	}

/** 
Whilst the following **forth_free** is not strictly necessary, there
is often a debate that comes up making short lived programs or programs whose
memory use stays either constant or only goes up, when these programs exit
it is not necessary to clean up the environment and in some case (although
not this one) it can slow down the exit of the program for
no reason.  However not freeing the memory after use does not play nice with
programs that detect memory leaks, like Valgrind. Either way, we free the
memory used here, but only if no other errors have occurred before hand. 
**/

	forth_free(o);
	return rval;
}

