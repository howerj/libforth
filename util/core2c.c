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

int readcell(forth_cell_t *out)
{
	return fread(out, sizeof(*out), 1, stdin);
}

int main(void)
{
	unsigned mod = 1;
	forth_cell_t cell;
	forth_cell_t size = 0;

	uint8_t discard[8 + 8] = {0};
	fread(discard, sizeof(discard), 1, stdin);

	puts("#include <libforth.h>");
	puts("forth_cell_t forth_core_data[] = {");
	while(1 == readcell(&cell)) {
		size++;
		printf("0x%0*"PRIxCell", ", (int)(sizeof(forth_cell_t)*2), cell);
		if(!(mod++ % 4))
			putchar('\n');
	}
	puts("\n};");
	printf("forth_cell_t forth_core_size = %zu;\n", size * sizeof(forth_cell_t));
	puts("");
	return 0;
}

