#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

static const char *corefile = "forth.core";
static const char *startfile = "forth.4th";

static FILE *fopen_or_fail(const char *name, char *mode){
        FILE *file;
        if(NULL == (file = fopen(name, mode))){
                perror(name);
                exit(EXIT_FAILURE);
        }
        return file;
}

int main(void)
{
        forth_obj_t *tobj;
        FILE *input, *core;
        input = fopen_or_fail(startfile, "rb");
        if(forth_run(tobj = forth_init(input, stdout)) < 0)
                return EXIT_FAILURE;
        forth_setin(tobj, stdin);
        if(forth_run(tobj) < 0)
                return EXIT_FAILURE;
        core = fopen_or_fail(corefile, "wb");
        if(forth_save(tobj, core) < 0)
                return EXIT_FAILURE;
        free(tobj);
        return EXIT_SUCCESS;
}
