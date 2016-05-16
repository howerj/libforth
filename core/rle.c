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

struct rle {
	jmp_buf *jb;
	FILE *in, *out;
	size_t read, wrote;
	uint32_t hash;
};

uint32_t djb2(uint8_t *blk, size_t len)
{
	uint32_t hash = 5381, c;
	for(c = *blk++; len--; c = *blk++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}

static int may_fgetc(struct rle *r)
{
	int rval = fgetc(r->in);
	r->read += rval != EOF;
	return rval;
}

static int expect_fgetc(struct rle *r)
{
	errno = 0;
	int c = fgetc(r->in);
	if(c == EOF) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		fprintf(stderr, "error: expected more input, %s\n", reason);
		longjmp(*r->jb, ERROR_IN_EOF);
	}
	r->read++;
	return c;
}

static int must_fputc(struct rle *r, int c)
{
	errno = 0;
	if(c != fputc(c, r->out)) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		fprintf(stderr, "error: could not write '%d' to output, %s\n", c, reason);
		longjmp(*r->jb, ERROR_OUT_EOF);
	}
	r->wrote++;
	return c;
}

static inline void increment(struct rle *r, char rw, size_t size)
{
	if(rw == 'w')
		r->wrote += size;
	else
		r->read  += size;
}

static size_t must_block_io(struct rle *r, void *p, size_t characters, char rw)
{ /* feof? */
	errno = 0;
	assert((rw == 'w') || (rw == 'r'));
	FILE *file = rw == 'w' ? r->out : r->in;
	size_t ret = rw == 'w' ? fwrite(p, 1, characters, file) : fread(p, 1, characters, file);
	if(ret != characters) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		char *type   = rw == 'w' ? "write" : "read";
		fprintf(stderr, "error: could not %s block, %s\n", type, reason);
		increment(r, rw, ret);
		longjmp(*r->jb, ERROR_OUT_EOF);
	}
	increment(r, rw, ret);
	return ret;
}

static void encode(struct rle *r)
{ /* needs more testing */
	uint8_t buf[128];
	int idx = 0;
	for(int c, j, prev = EOF; (c = may_fgetc(r)) != EOF; prev = c) {
		if(c == prev) {
			if(idx > 1) {
				must_fputc(r, idx+128);
				must_block_io(r, buf, idx, 'w');
				idx = 0;
			} 
			for(j = 0; (c = may_fgetc(r)) == prev && j < 128; j++)
				;
			must_fputc(r, j);
			must_fputc(r, prev);
			if(c == EOF) {
				goto end;
			}
		}
		buf[idx++] = c;
		if(idx == 127) {
			must_fputc(r, idx+128);
			must_block_io(r, buf, idx, 'w');
			idx = 0;
		}
		assert(idx < 127);
	}
end:
	if(idx) {
		must_fputc(r, idx+128);
		must_block_io(r, buf, idx, 'w');
		idx = 0;
	}

}

int run_length_encoder(int verbose, FILE *in, FILE *out)
{
	jmp_buf jb;
	int rval;
	if((rval = setjmp(jb)))
		return rval;
	struct rle r = { &jb, in, out, 0, 0, 0};
	must_block_io(&r, header, sizeof(header), 'w');
	encode(&r);
	if(verbose)
		fprintf(stderr, "encoded: read %zu wrote %zu\n", r.read, r.wrote);
	return OK;
}

static void decode(struct rle *r)
{
	for(int c, count; (c = may_fgetc(r)) != EOF;) {
		if(c > 128) {
			count = c - 128;
			for(int i = 0; i < count; i++) {
				c = expect_fgetc(r);
				must_fputc(r, c);
			}
		} else {
			count = c;
			c = expect_fgetc(r);
			for(int i = 0; i < count + 1; i++)
				must_fputc(r, c);
		}
	}
}

int run_length_decoder(int verbose, FILE *in, FILE *out)
{
	char head[sizeof(header)] = {0};
	jmp_buf jb;
	int rval;
	if((rval = setjmp(jb)))
		return rval;
	struct rle r = { &jb, in, out, 0, 0, 0 };
	must_block_io(&r, head, sizeof(head), 'r');
	if(memcmp(head, header, sizeof(head))) {
		fprintf(stderr, "error: invalid header\n");
		return ERROR_INVALID_HEADER;
	}
	decode(&r);
	if(verbose)
		fprintf(stderr, "decoded: read %zu wrote %zu\n", r.read, r.wrote);
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
	int mode = 0, r = ERROR_ARG, i, verbose = 0;
	for(i = 1; i < argc && argv[i][0] == '-'; i++) {
		switch(argv[i][1]) {
		case '\0': goto done;
		case 'v':  verbose++; break;
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
		r = run_length_encoder(verbose, in, out);
	if(mode == 2)
		r = run_length_decoder(verbose, in, out);
	fclose(in);
	fclose(out);
	return r;

fail:
	fprintf(stderr, "usage %s [-(e|d)] [-v] file.in file.out\n", argv[0]);
	return ERROR_ARG;
}

