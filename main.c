#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

int main(void)
{
	int r;
        forth_obj_t *tobj = forth_init(stdin, stdout);
	if(!tobj)
		return -1;
        r = forth_run(tobj);
        free(tobj);
        return r;
}
