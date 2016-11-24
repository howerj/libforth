#include <libforth.h>
#include <stdio.h>

int main(void)
{
	FILE *core = fopen("forth.core", "rb");
	forth_t *f = NULL;
	int r = 0;
	if(core) {
		f = forth_load_core_file(core);
		fclose(core);
	}
	if(!f)
		f = forth_init(32 * 1024, stdin, stdout, NULL);
	if(!f) 
		return -1;
	if((r = forth_run(f)) < 0)
		return r;
	if(!(core = fopen("forth.core", "wb")))
		return -1;
	return forth_save_core_file(f, core);
}

