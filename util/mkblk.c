/** @file       mkblk.c
 *  @brief      Make and manipulate forth blocks
 *  @author     Richard James Howe.
 *  @copyright  Copyright 2015,2016 Richard James Howe.
 *  @license    LGPL v2.1 or later version
 *  @email      howe.r.j.89@gmail.com 
 *  @todo split a file into forth blocks, padding if needed 
 *  @todo option to validate forth block length and name of an existing block*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define BLOCK_SIZE (1024)
#define MAX_NUMBER (0xFFFFu)

static int split_counter = 0;

void usage(char *name)
{
	static const char *help = "\
make forth blocks\n\
usage %s [-h] [-z] [-v] [-s file] number...\n\n\
	-h      print this help and exit unsuccessfully\n\
	-z      zero the block instead of writing space to it\n\
	-v      verbose mode\n\
	-s      split file into blocks, padded with zeros or spaces, then exit\n\
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

static FILE *fopen_or_die(const char *name, char *mode)
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

static char *block_name(int blkno)
{
	static char name[32];
	memset(name, 0, sizeof(name));
	sprintf(name, "%04x.blk", (int)blkno);
	return name;
}

static void make_block(int blkno, int zero)
{
	char *name = block_name(blkno);
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

static int split(const char *name, int zero)
{
	FILE *in = fopen_or_die(name, "rb");
	static uint8_t blk[1024];

	memset(blk, zero ? 0 : ' ', sizeof(blk));
	while(fread(blk, 1, sizeof(blk), in))
	{
		char *blkname = block_name(split_counter++);
		FILE *out = fopen_or_die(blkname, "wb");
		if(sizeof(blk) != fwrite(blk, 1, sizeof(blk), out))
		{
			fprintf(stderr, "error: could not write block %s for file %s\n", blkname, name);
			abort();
		}
		fclose(out);
		memset(blk, zero ? 0 : ' ', sizeof(blk));
	}
	fclose(in);
	return 0;
}

int main(int argc, char **argv)
{
	int i, zero = 0, verbose = 0;
	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		switch(argv[i][1]) {
			case '\0': goto done;
			case 'h':  usage(argv[0]); return -1;
			case 'z':  zero    = 1; break;
			case 'v':  verbose = 1; break;
			case 's':  
				   if(i >= (argc - 1))
				   {
					   fprintf(stderr, "error: -s expects file name\n");
					   usage(argv[0]);
					   return -1;
				   }
				   return split(argv[++i], zero);
				   break;
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
