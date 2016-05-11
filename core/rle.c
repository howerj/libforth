/** @file       rle.c
 *  @brief      a simple run length encoder
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @warning    this is a work in progress
 *  @todo       better command line arguments
 *  @todo       append and read in a header
 *  @todo       turn into library (no calls to exit, read/write to strings
 *              as well, unit tests, man pages)
 *  This is a run length encoder, which should be good for compressing Forth
 *  core files. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <stdint.h>

enum errors { OK = 0, ERROR_IN_EOF = -1, ERROR_OUT_EOF = -2, ERROR_ARG = -3, ERROR_FILE = -4};

static int expect_fgetc(FILE *in) 
{
	errno = 0;
	int c = fgetc(in);
	char *reason = errno ? strerror(errno) : "(unknown reason)";
	if(c == EOF) {
		fprintf(stderr, "error: expected more input, %s\n", reason);
		exit(ERROR_IN_EOF);
	}
	return c;
}

static int must_fputc(int c, FILE *out)
{
	errno = 0;
	char *reason = errno ? strerror(errno) : "(unknown reason)";
	if(c != fputc(c, out)) {
		fprintf(stderr, "error: could not write '%d' to output, %s\n", c, reason);
		exit(ERROR_OUT_EOF);
	}
	return c;
}

static size_t must_write_block(const void *p, size_t characters, FILE *out)
{
	errno = 0;
	size_t ret = fwrite(p, 1, characters, out);
	char *reason = errno ? strerror(errno) : "(unknown reason)";
	if(ret != characters) {
		fprintf(stderr, "error: could not write block to output, %s\n", reason);
		exit(ERROR_OUT_EOF);
	}
	return ret;
}

static FILE *fopen_or_die(char *name, char *mode)
{
	errno = 0;
	FILE *ret = fopen(name, mode);
	char *reason = errno ? strerror(errno) : "could not open file (unknown reason)";
	if(!ret) {
		fprintf(stderr, "%s:%s\n", name, reason);
		exit(ERROR_FILE);
	}
	return ret;
}

static void encode(FILE *in, FILE *out)
{ /* needs more testing */
	uint8_t buf[256];
	for(int c, idx = 0, j, prev = EOF; (c = fgetc(in)) != EOF; prev = c) {
		if(c == prev) {
			if(idx) {
				must_fputc(idx+128, out);
				must_write_block(buf, idx, out);
				idx = 0;
			}
			for(j = 0; (c = fgetc(in)) == prev && j < 129; j++)
				;
			must_fputc(j,    out);
			must_fputc(prev, out);
		}
		buf[idx++] = c;
		if(idx == 128) {
			must_fputc(idx, out);
			must_write_block(buf, idx, out);
			idx = 0;
		}
		assert(idx < 128);
	}
}

static void decode(FILE *in, FILE *out)
{
	for(int c, count; (c = fgetc(in)) != EOF;) {
		if(c > 128) {
			count = c - 128;
			for(int i = 0; i < count; i++) {
				c = expect_fgetc(in);
				must_fputc(c, out);
			}
		} else {
			count = c;
			c = expect_fgetc(in);
			for(int i = 0; i < count + 1; i++)
				must_fputc(c, out);
		}
	}
}

int main(int argc, char **argv)
{ 
	FILE *in = NULL, *out = NULL;
	int mode = 0;
	if(argc != 4)
		goto fail;

	if(!strcmp(argv[1], "-e"))
		mode = 1;
	else if(!strcmp(argv[1], "-d"))
		mode = 0;
	else
		goto fail;

	in  = fopen_or_die(argv[2], "rb");
	out = fopen_or_die(argv[3], "wb");

	if(mode)
		encode(in, out);
	else
		decode(in, out);

	fclose(in);
	fclose(out);
	return OK;
fail:
	if(in)
		fclose(in);
	if(out)
		fclose(out);
	fprintf(stderr, "usage %s -(e|d) file.in file.out\n", argv[0]);
	return ERROR_ARG;
}

