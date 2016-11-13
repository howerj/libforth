/**
@brief  Test program to interact with a Forth Linux Kernel Module
@author Richard James Howe
@email  howe.r.j.89@gmail.com
@license MIT
**/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libline.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_HIST_LINES (100u)
#define INPUT_BUFFER_LENGTH (256)

typedef enum {
	LOG_LEVEL_ALL_OFF,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_NOTE,
	LOG_LEVEL_DEBUG, /* make an option to run a monitor if in this mode when _logger encountered? */
	LOG_LEVEL_ALL_ON
} log_level_e;

static const char *histfile = "line.log";
static bool hist_enabled = true;
static const char *device = "/dev/forth";
static const char *prompt = "> ";
static log_level_e log_level = LOG_LEVEL_WARNING;
static unsigned line_editor_mode = 0; /* normal mode, emacs mode, set to 1 for vi */
static bool die_on_warnings = 0;

static const char *emsg(void)
{
	static const char *unknown = "unknown reason";
	const char *r = errno ? strerror(errno) : unknown;
	if(!r) 
		r = unknown;
	return r;
}

#define error(FMT, ...)   _logger(true,            LOG_LEVEL_ERROR,   __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define warning(FMT, ...) _logger(die_on_warnings, LOG_LEVEL_WARNING, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define note(FMT, ...)    _logger(false,           LOG_LEVEL_NOTE,    __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define debug(FMT, ...)   _logger(false,           LOG_LEVEL_DEBUG,   __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)

int _logger(bool die, log_level_e level, const char *file, const char *func, unsigned line, const char *fmt, ...)
{
	int r = 0;
	va_list args;
	assert(file && func && fmt);
	if(level > log_level) 
		goto end;
	if((r = fprintf(stderr, "[%s %s %u] ", file, func, line)) < 0)
		goto end;
	va_start(args, fmt);
	if((r = vfprintf(stderr, fmt, args)) < 0)
		goto cleanup;
	if((r = fputc('\n', stderr)) != '\n')
		goto cleanup;
cleanup:
	va_end(args);
end:
	if(die)
		exit(EXIT_FAILURE);
	return r;
}

int command(int fd, const char *line, FILE *output)
{
	int r = -1;
	size_t length;
	static uint8_t rbuf[INPUT_BUFFER_LENGTH];
	assert(line && output);
	length = strlen(line) + 1;
	if(length > (INPUT_BUFFER_LENGTH - 1)) {
		warning("input line too long (%zu > %d)\n", length, INPUT_BUFFER_LENGTH - 1);
		return -1;
	}
	memset(rbuf, 0, INPUT_BUFFER_LENGTH);

	errno = 0;
	r = write(fd, line, length);
	if(r < 0) {
		warning("command returned: %s", emsg());
		return -1;
	}
	note("write succeeded");
	
	errno = 0;
	r = read(fd, rbuf, INPUT_BUFFER_LENGTH);
	if(r < 0) {
		warning("read returned: %s", emsg());
	}
	note("read succeeded");
	fprintf(output, "%.*s\n", INPUT_BUFFER_LENGTH - 1 , rbuf);
	return 0;
}

static void print_usage(const char *prog)
{
	assert(prog);
	fprintf(stderr, "usage: %s [-hvHViD]\n", prog);
	fprintf(stderr, "Test program to exercise device %s.\n", device);
	fputs("  -h --help     print out this help message and exit\n"
	      "  -v --verbose  increase verbosity level\n"
	      "  -H --history  set the history file\n"
	      "  -V --vi       set line editor to vi mode\n"
	      "  -d --device   set device to read\n"
	      "  -D --die      die if a warning occurs\n"
	      , stderr);
	exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char *argv[])
{
	assert(argv);
	static const struct option lopts[] = {
		{ "help",    0, 0, 'h' },
		{ "verbose", 0, 0, 'v' },
		{ "vi",      0, 0, 'V' },
		{ "history", 1, 0, 'H' },
		{ "die",     0, 0, 'D' },
		{ "device",  1, 0, 'd' },
		{ NULL,      0, 0, 0 },
	};
	int c;

	while ((c = getopt_long(argc, argv, "hvVH:Di:", lopts, NULL)) != -1) {
		switch (c) {
		case 'v':
			log_level = log_level > LOG_LEVEL_ALL_ON ? log_level : log_level + 1;
			break;
		case 'V':
			line_editor_mode = 1;
			break;
		case 'H':
			histfile = optarg;
			break;
		case 'D':
			die_on_warnings = true;
			break;
		case 'd':
			device = optarg;
			break;
		default: 
			warning("invalid option %c", c);
			/*fall through*/
		case 'h':
			print_usage(argv[0]);
			break;
		}
	}
}

int main(int argc, char **argv)
{
	int fd = -1;
	char *line = NULL;

	parse_opts(argc, argv);
	line_set_vi_mode(!!line_editor_mode);

	note("mode      = %s", line_editor_mode ? "vi" : "emacs");
	note("verbosity = %d", log_level); /* yes, this is intentional */

	if(hist_enabled) {
		errno = 0;
		/* fails if file does not exist, for example on the first run */
		if(line_history_load(histfile) < 0)
			note("%s failed to load: %s", histfile, emsg());

		if(line_history_set_maxlen(MAX_HIST_LINES) < 0) {
			warning("could not set history file size to %u", MAX_HIST_LINES);
			hist_enabled = false;
		}
	}

	errno = 0;
	fd = open(device, O_RDWR);
	if(fd == -1)
		error("could not open %s for reading: %s", device, emsg());

	while((line = line_editor(prompt))) {
		debug("%s", line);
		if(command(fd, line, stdout) < 0) {
			error("invalid command: exiting");
			goto fail;
		}

		if(hist_enabled && line_history_add(line) < 0) {
			warning("could not add line to history, disabling history feature");
			hist_enabled = false;
		}
		free(line);
	}

fail:
	free(line);

	errno = 0;
	if(close(fd) == -1)
		warning("could not close file handle: %s", emsg());

	errno = 0;
	if(hist_enabled && line_history_save(histfile) < 0)
		warning("could not save history to %s: %s", histfile, emsg());

	return 0;
}

