/** 
@brief Turn a forth.core file into a C file
@todo translate this into a Forth program 
@note it would be nice if the generated file was compressed, of course the
consuming program would have to be able to decompress the data. Simple Run
Length Compression would save a lot of space. */
#include <stdio.h>
#include <libforth.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>

int main(int argc, char **argv)
{
	unsigned mod = 1;
	int c;
	forth_cell_t size = 0;

	if(argc != 3) {
		fprintf(stderr, "usage: %s input.bin output.c\n", argv[0]);
		return -1;
	}

	FILE *in = forth_fopen_or_die(argv[1], "rb"), 
	     *out = forth_fopen_or_die(argv[2], "wb");

	fputs("#include <libforth.h>\n", out);
	fputs("unsigned char forth_core_data[] = {\n", out);
	while(EOF != (c = fgetc(in))) {
		size++;
		fprintf(out, "0x%02x"", ", c);
		if(!(mod++ % 16))
			fputc('\n', out);
	}
	fputs("\n};", out);
	fprintf(out, "forth_cell_t forth_core_size = %zu;\n", size);
	fputs("\n", out);
	return 0;
}

