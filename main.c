#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

int main(void)
{
        forth_obj_t *tobj;
        FILE *input;
        if(NULL == (input = fopen("forth.4th", "rb"))){
                perror("forth.4th");
                return -1;
        }
        if(forth_run(tobj = forth_init(input, stdout)) < 0)
                return -1;
        forth_setin(tobj, stdin);
        if(forth_run(tobj) < 0)
                return -1;
        free(tobj);
        return 0;
}
