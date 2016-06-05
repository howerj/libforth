/** @file       hexdump.c
 *  @brief      a simple hex dump utility
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static void fail_if(int test, const char *msg)
{
	if (test) {
		if (errno && msg)
			perror(msg);
		else if (msg)
			fputs(msg, stderr);
		exit(-1);
	}
}

static FILE *fopen_or_fail(const char *name, const char *mode)
{
	FILE *f = NULL;
	fail_if(!(f = fopen(name, mode)), name);
	return f;
}

static void hexdump_inner(FILE * input)
{
	int c;
	unsigned long long counter = 0;
	while (EOF != (c = fgetc(input))) {
		if (!(counter++ % 16))
			printf("\n%011llx ", counter - 1);
		printf("%02x ", c);
	}
	putchar('\n');
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		int i;
		for (i = 1; i < argc; i++) {
			FILE *input = fopen_or_fail(argv[i], "rb");
			printf("# %s", argv[i]);
			hexdump_inner(input);
			fclose(input);
		}
	} else {
		hexdump_inner(stdin);
	}
	return 0;
}

