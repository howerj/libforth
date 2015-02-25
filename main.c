#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

int main(void)
{
	int r;
        forth_obj_t *tobj;
        r = forth_run(tobj = forth_init(fopen("forth.4th","rb"), stdout));
        forth_setin(tobj,stdin);
        forth_run(tobj);
        free(tobj);
        return r;
}
