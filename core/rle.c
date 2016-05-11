/** @file       rle.c
 *  @brief      a simple run length encoder
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @warning    this is a work in progress
 *  @todo       add checksum, length and whether encoded succeeded (size decreased) to header
 *  @todo       turn into library (read/write to strings as well as files, unit tests, man pages)
 *  This is a run length encoder, which should be good for compressing Forth core files. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>
#include <stdint.h>

#define VERSION (0x1)

enum errors { OK = 0, ERROR_IN_EOF = -1, ERROR_OUT_EOF = -2, ERROR_ARG = -3, ERROR_FILE = -4, ERROR_INVALID_HEADER = -5};

static char header[16] = { 0xFF, 'R', 'L', 'E', VERSION /*add simple checksum and length*/};

static int expect_fgetc(jmp_buf *jb, FILE *in) 
{
	errno = 0;
	int c = fgetc(in);
	if(c == EOF) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		fprintf(stderr, "error: expected more input, %s\n", reason);
		longjmp(*jb, ERROR_IN_EOF);
	}
	return c;
}

static int must_fputc(jmp_buf *jb, int c, FILE *out)
{
	errno = 0;
	if(c != fputc(c, out)) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		fprintf(stderr, "error: could not write '%d' to output, %s\n", c, reason);
		longjmp(*jb, ERROR_OUT_EOF);
	}
	return c;
}

static size_t must_block_io(jmp_buf *jb, void *p, size_t characters, FILE *file, char rw)
{
	errno = 0;
	assert((rw == 'w') || (rw == 'r'));
	size_t ret = rw == 'w' ? fwrite(p, 1, characters, file) : fread(p, 1, characters, file);
	if(ret != characters) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		char *type   = rw == 'w' ? "write" : "read";
		fprintf(stderr, "error: could not %s block, %s\n", type, reason);
		longjmp(*jb, ERROR_OUT_EOF);
	}
	return ret;
}

static void encode(jmp_buf *jb, FILE *in, FILE *out)
{ /* needs more testing */
	uint8_t buf[256];
	for(int c, idx = 0, j, prev = EOF; (c = fgetc(in)) != EOF; prev = c) {
		if(c == prev) {
			if(idx) {
				must_fputc(jb, idx+128, out);
				must_block_io(jb, buf, idx, out, 'w');
				idx = 0;
			}
			for(j = 0; (c = fgetc(in)) == prev && j < 129; j++)
				;
			must_fputc(jb, j,    out);
			must_fputc(jb, prev, out);
		}
		buf[idx++] = c;
		if(idx == 128) {
			must_fputc(jb, idx, out);
			must_block_io(jb, buf, idx, out, 'w');
			idx = 0;
		}
		assert(idx < 128);
	}
}

int run_length_encoder(FILE *in, FILE *out)
{
	jmp_buf jb;
	int r;
	if((r = setjmp(jb)))
		return r;
	must_block_io(&jb, header, sizeof(header), out, 'w');
	encode(&jb, in, out);
	return OK;
}

static void decode(jmp_buf *jb, FILE *in, FILE *out)
{
	for(int c, count; (c = fgetc(in)) != EOF;) {
		if(c > 128) {
			count = c - 128;
			for(int i = 0; i < count; i++) {
				c = expect_fgetc(jb, in);
				must_fputc(jb, c, out);
			}
		} else {
			count = c;
			c = expect_fgetc(jb, in);
			for(int i = 0; i < count + 1; i++)
				must_fputc(jb, c, out);
		}
	}
}

int run_length_decoder(FILE *in, FILE *out)
{
	char head[sizeof(header)] = {0};
	jmp_buf jb;
	int r;
	if((r = setjmp(jb)))
		return r;
	must_block_io(&jb, head, sizeof(head), in, 'r');
	if(memcmp(head, header, sizeof(head))) {
		fprintf(stderr, "error: invalid header\n");
		return ERROR_INVALID_HEADER;
	}
	decode(&jb, in, out);
	return OK;
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

int main(int argc, char **argv)
{ 
	FILE *in  = stdin, *out  = stdout;
	char *cin = NULL,  *cout = NULL;
	int mode = 0, r = ERROR_ARG, i;
	for(i = 1; i < argc && argv[i][0] == '-'; i++) {
		switch(argv[i][1]) {
		case '\0': goto done;
		case 'e':  if(mode) goto fail; mode = 1; break;
		case 'd':  if(mode) goto fail; mode = 2; break;
		default:   goto fail;
		}
	}
done:
	if(i < argc)
		cin  = argv[i++];
	if(i < argc)
		cout = argv[i++];
	if(i < argc)
		goto fail;
	if(cin)
		in  = fopen_or_die(cin,  "rb");
	if(cout)
		out = fopen_or_die(cout, "wb");

	if(!mode) /*default to encode*/
		mode = 1;

	if(mode == 1)
		r = run_length_encoder(in, out);
	if(mode == 2)
		r = run_length_decoder(in, out);
	fclose(in);
	fclose(out);
	return r;

fail:
	fprintf(stderr, "usage %s -(e|d) file.in file.out\n", argv[0]);
	return ERROR_ARG;
}

