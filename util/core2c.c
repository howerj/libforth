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

int main(void)
{
	unsigned mod = 1;
	int c;
	forth_cell_t size = 0;

	puts("#include <libforth.h>");
	puts("unsigned char forth_core_data[] = {");
	while(EOF != (c = fgetc(stdin))) {
		size++;
		printf("0x%02x"", ", c);
		if(!(mod++ % 16))
			putchar('\n');
	}
	puts("\n};");
	printf("forth_cell_t forth_core_size = %zu;\n", size);
	puts("");
	return 0;
}

