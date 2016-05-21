#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define BLOCK_SIZE (1024)
#define MAX_NUMBER (0xFFFFu)

void usage(char *name)
{
	static const char *help = "\
make forth blocks\n\
usage %s [-h] [-z] [-v] number...\n\n\
	-h      print this help and exit unsuccessfully\n\
	-z      zero the block instead of writing space to it\n\
	-v      verbose mode\n\
	number  make a block named with 'number'\n\n\
This program makes valid blocks which can be loaded by the forth\n\
interpreter program, blocks are files containing 1024 bits of data,\n\
given no arguments this program will create a block called '0000.blk'\n\
containing only spaces and newlines.\n\n\
";
	fprintf(stderr, help, name);
}

static void fputc_or_die(int c, FILE *out)
{
	errno = 0;
	if(c != fputc(c, out)) {
		fprintf(stderr, "fputc: %s\n", errno ? strerror(errno) : "could not write character");
		abort();
	}
}

static FILE *fopen_or_die(char *name, char *mode)
{
	errno = 0;
	FILE *ret = fopen(name, mode);
	char *reason = errno ? strerror(errno) : "could not open file (unknown reason)";
	if(!ret) {
		fprintf(stderr, "%s: %s\n", name, reason);
		abort();
	}
	return ret;
}

static void make_block(int blkno, int zero)
{
	char name[32] = {0};
	sprintf(name, "%04x.blk", (int)blkno);
	FILE *out = fopen_or_die(name, "wb");
	if(zero) {
		for(int i = 0; i < BLOCK_SIZE; i++)
			fputc_or_die(0, out);
	} else {
		for(int i = 0; i < BLOCK_SIZE/64; i++) {
			for(int j = 0; j < 63; j++)
				fputc_or_die(' ', out);
			fputc_or_die('\n', out);
		}
	}
	fclose(out);
}

static int numberify(long *n, const char *s)  
{ /*returns non zero if conversion was successful*/
	char *end = NULL;
	errno = 0;
	*n = strtol(s, &end, 0);
	return !errno && *s != '\0' && *end == '\0';
}

static long number_or_die(const char *s)
{
	long n = 0;
	if(!numberify(&n, s)) {
		fprintf(stderr, "\"%s\": %s\n", s, errno ? strerror(errno) : "could not convert to number");
		abort();
	}
	return n;
}

int main(int argc, char **argv)
{
	int i, zero = 0, verbose = 0;
	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		switch(argv[i][1]) {
			/**@todo option to validate forth block length and name 
			 * of an existing block */
			case '\0': goto done;
			case 'h':  usage(argv[0]); return -1;
			case 'z':  zero    = 1; break;
			case 'v':  verbose = 1; break;
			default:
				   fprintf(stderr, "error: invalid argument '%s'\n", argv[i]);
				   usage(argv[0]);
				   return -1;
		}
done:
	if(verbose && zero)
		fputs("zeroing blocks", stderr);
	if(i < argc)
		for(; i < argc; i++)
			make_block(number_or_die(argv[i]) % MAX_NUMBER, zero);
	else
		make_block(0, zero);
	return 0;
}
