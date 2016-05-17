/** @file       rle.c
 *  @brief      a simple run length encoder
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @bug        fix encoder so it only encodes what it has to and not any more,
 *              this does not make the output stream invalid, only larger than
 *              it should be, this affects large runs of bytes that are the
 *              same
 *  @todo       add checksum, length and whether encoded succeeded (size decreased) to header
 *  @todo       turn into library (read/write to strings as well as files, man pages)
 * 
 * The encoder works on binary data, it encodes successive runs of
 * bytes as a length and the repeated byte up to runs of 127, or
 * a differing byte is found. This is then emitted as two bytes;
 * the length and the byte to repeat. Input data with no runs in
 * it is encoded as the length of the data that differs plus that
 * data, up to 128 bytes can be encoded this way.
 **/
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define  VERSION            (0x2)
#define  INVALID_HASH       (0xFFFFu) /* fletcher16 hash can never be this value */
#define  INVALID_SIZE_FIELD (0x0)
#define  INVALID_HASH_FIELD (0x0)

enum mode { InvalidMode, DecodeMode, EncodeMode };

enum errors { 
	Ok                  =   0, /* everything is groovy */
	ErrorHelp           =  -1, /* used to exit unsuccessfully after printing help message*/
	ErrorInEoF          =  -2, /* expected input but got an EOF */
	ErrorOutEoF         =  -3, /* expected to output bytes, but encountered and error */
	ErrorArg            =  -4, /* invalid command line args */
	ErrorFile           =  -5, /* could not open file, for whatever reason */
	ErrorInvalidHeader  =  -6  /* invalid header */
};

enum header {
	eMagic0,     eMagic1,     eMagic2,     eMagic3,
	eVersion,    eReserved0,  eReserved1,  eEndMagic,
	eSizeValid,  eHashValid,  eHash0,      eHash1,
	eReserved2,  eReserved3,  eReserved4,  eReserved5,
	eSize0,      eSize1,      eSize2,      eSize3,   
	eSize4,      eSize5,      eSize6,      eSize7
};

static uint8_t header[24] = { 
	0xFF,    'R', 'L', 'E',   /* magic numbers to identify file type */
	VERSION,  0,   0,   0xFF, /* file version number, reserved bytes, end byte */
	INVALID_SIZE_FIELD, INVALID_HASH_FIELD, (uint8_t)INVALID_HASH, (uint8_t)(INVALID_HASH >> 8),
	0,        0,   0,   0,    /* reserved */

	0,        0,   0,   0,    /* size */
	0,        0,   0,   0,    /* size */
};

struct rle {
	enum mode mode;       /* mode of operation */
	jmp_buf *jb;          /* place to jump on error */
	FILE *in, *out;       /* input and output files */
	uint64_t read, wrote; /* bytes read in and wrote */
	uint16_t hash;        /* hash of original data */
};

static inline uint16_t fletcher16(uint8_t *data, size_t count)
{ /* https://en.wikipedia.org/wiki/Fletcher%27s_checksum */
	uint16_t x = 0, y = 0;
	size_t i;
	/* this function needs splitting into a start function,
	 * a hash function and an end function */
	for(i = 0; i < count; i++) {
		x = (x + data[i]) & 255;
		y = (y + x) & 255;
	}
	return (y << 8) | x;
}

static inline int may_fgetc(struct rle *r)
{
	int rval = fgetc(r->in);
	r->read += rval != EOF;
	return rval;
}

static inline int expect_fgetc(struct rle *r)
{
	errno = 0;
	int c = fgetc(r->in);
	if(c == EOF) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		fprintf(stderr, "error: expected more input, %s\n", reason);
		longjmp(*r->jb, ErrorInEoF);
	}
	r->read++;
	return c;
}

static inline int must_fputc(struct rle *r, int c)
{
	errno = 0;
	if(c != fputc(c, r->out)) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		fprintf(stderr, "error: could not write '%d' to output, %s\n", c, reason);
		longjmp(*r->jb, ErrorOutEoF);
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

static inline size_t must_block_io(struct rle *r, void *p, size_t characters, char rw)
{
	errno = 0;
	assert((rw == 'w') || (rw == 'r'));
	FILE *file = rw == 'w' ? r->out : r->in;
	size_t ret = rw == 'w' ? fwrite(p, 1, characters, file) : fread(p, 1, characters, file);
	if(ret != characters) {
		char *reason = errno ? strerror(errno) : "(unknown reason)";
		char *type   = rw == 'w' ? "write" : "read";
		fprintf(stderr, "error: could not %s block, %s\n", type, reason);
		increment(r, rw, ret);
		longjmp(*r->jb, ErrorOutEoF);
	}
	increment(r, rw, ret);
	return ret;
}

static inline void must_encode_buf(struct rle *r, uint8_t *buf, int *idx)
{
	must_fputc(r, (*idx)+128);
	must_block_io(r, buf, *idx, 'w');
	*idx = 0;
}

static void print_results(int verbose, struct rle *r, int encode)
{
	if(!verbose)
		return;
	fputs(encode ? "encode:\n" : "decode:\n", stderr);
	fprintf(stderr, "\tread   %"PRIu64"\n", r->read);
	fprintf(stderr, "\twrote  %"PRIu64"\n", r->wrote);
	fprintf(stderr, "\thash   %"PRIu16"\n", r->hash);
}

static void encode(struct rle *r)
{ /* This, whilst producing a correct stream, produces more than it should */
	uint8_t buf[128];
	int idx = 0;
	for(int c, j, prev = EOF; (c = may_fgetc(r)) != EOF; prev = c) {
		if(c == prev) {
			if(idx > 1)
				must_encode_buf(r, buf, &idx);
			for(j = 0; (c = may_fgetc(r)) == prev && j < 128; j++)
				;
			must_fputc(r, j);
			must_fputc(r, prev);
			if(c == EOF) {
				goto end;
			}
		}
		buf[idx++] = c;
		if(idx == 127)
			must_encode_buf(r, buf, &idx);
		assert(idx < 127);
	}
end:
	if(idx)
		must_encode_buf(r, buf, &idx);
}

int run_length_encoder(int headerless, int verbose, FILE *in, FILE *out)
{
	jmp_buf jb;
	int rval;
	if((rval = setjmp(jb)))
		return rval;
	struct rle r = { EncodeMode, &jb, in, out, 0, 0, 0};
	if(!headerless)
		must_block_io(&r, header, sizeof(header), 'w');
	encode(&r);
	print_results(verbose, &r, 1);
	return Ok;
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

int run_length_decoder(int headerless, int verbose, FILE *in, FILE *out)
{
	char head[sizeof(header)] = {0};
	jmp_buf jb;
	int rval;
	if((rval = setjmp(jb)))
		return rval;
	struct rle r = { DecodeMode, &jb, in, out, 0, 0, 0};
	if(!headerless) {
		must_block_io(&r, head, sizeof(head), 'r');
		if(memcmp(head, header, eEndMagic+1)) {
			fprintf(stderr, "error: invalid header\n");
			return ErrorInvalidHeader;
		}
	}
	decode(&r);
	print_results(verbose, &r, 0);
	return Ok;
}

static FILE *fopen_or_die(char *name, char *mode)
{
	errno = 0;
	FILE *ret = fopen(name, mode);
	char *reason = errno ? strerror(errno) : "could not open file (unknown reason)";
	if(!ret) {
		fprintf(stderr, "%s: %s\n", name, reason);
		exit(ErrorFile);
	}
	return ret;
}

static void help(void)
{
	static const char h[] =
"\nRun Length Encoder and Decoder\n\n\
	-e\tencode\n\
	-d\tdecode (mutually exclusive with '-e')\n\
	-v\tturn on verbose mode\n\
	-h\tprint help and exit unsuccessfully\n\
	-H\tdo not make or process a header\n\
	-\tstop processing command line options\n\n\
The file parameters are optional, with the possible combinations:\n\
\n\
\t0 files specified:\n\t\tinput:  stdin   \n\t\toutput: stdout\n\
\t1 file  specified:\n\t\tinput:  1st file\n\t\toutput: stdout\n\
\t2 files specified:\n\t\tinput:  1st file\n\t\toutput: 2nd file\n\
\n";
	fputs(h, stderr);
}

static void usage(char *name)
{
	fprintf(stderr, "usage %s [-(e|d)] [-h] [-H] [-v] [file.in] [file.out]\n", name);
}

int main(int argc, char **argv)
{ 
	FILE *in  = stdin, *out  = stdout;
	char *cin = NULL,  *cout = NULL;
	int r = ErrorArg, i, verbose = 0, headerless = 0;
	enum mode mode = InvalidMode;
	for(i = 1; i < argc && argv[i][0] == '-'; i++) {
		switch(argv[i][1]) {
		case '\0': goto done;
		case 'h':  usage(argv[0]); help(); return ErrorHelp;
		case 'v':  verbose    = 1; break;
		case 'H':  headerless = 1; break;
		case 'e':  if(mode) goto fail; mode = EncodeMode; break;
		case 'd':  if(mode) goto fail; mode = DecodeMode; break;
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
	if(verbose)
		fprintf(stderr, "input:  %s\n", cin  ? cin  : "/dev/stdin");
	if(verbose)
		fprintf(stderr, "output: %s\n", cout ? cout : "/dev/stdout");

	if(mode == InvalidMode) /*default to encode*/
		mode = EncodeMode;

	if(mode == EncodeMode)
		r = run_length_encoder(headerless, verbose, in, out);
	if(mode == DecodeMode)
		r = run_length_decoder(headerless, verbose, in, out);
	fclose(in);
	fclose(out);
	return r;

fail:
	usage(argv[0]);
	return ErrorArg;
}

