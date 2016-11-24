#include <libforth.h>
#include <stdio.h>

extern char forth_core_data[];
extern forth_cell_t forth_core_size;

int main(void)
{
	int r = 0;
	forth_t *o = forth_load_core_memory(forth_core_data, forth_core_size);
	if(!o) {
		fprintf(stderr, "loading forth core failed\n");
		return -1;
	}
	r = forth_run(o);
	return r;
}

