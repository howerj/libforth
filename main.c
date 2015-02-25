#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

int main(void)
{
	int r;
        forth_obj_t *tobj;
        r = forth_run(tobj = forth_init(stdin, stdout));
        free(tobj);
        return r;
}
