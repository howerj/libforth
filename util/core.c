#include <libforth.h>
#include <stdio.h>

#ifdef USE_OBJDUMP
extern forth_cell_t forth_core_data;
extern forth_cell_t forth_core_size;
#else
extern forth_cell_t forth_core_data[];
extern forth_cell_t forth_core_size;
#endif

int main(void)
{
	int r = 0;
#ifdef USE_OBJDUMP
	forth_cell_t  size = (forth_cell_t)(&forth_core_size);
	forth_cell_t *data = &forth_core_data;
	data += 2*(sizeof(forth_cell_t)/sizeof(uint64_t)); /* skip header */
	size -= 2*sizeof(forth_cell_t);
#else
	forth_cell_t  size = forth_core_size;
	forth_cell_t *data = forth_core_data;
#endif
	forth_t *o = forth_load_core_memory(data, size);
	if(!o) {
		fprintf(stderr, "loading forth core failed\n");
		return -1;
	}
	r = forth_run(o);
	return r;
}

