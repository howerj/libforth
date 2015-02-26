#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

static const char *coref = "forth.core", *startf = "forth.4th";

static FILE *fopen_or_fail(const char *name, char *mode)
{
        FILE *file;
        if(!(file = fopen(name, mode)))
                perror(name), exit(1);
        return file;
}

int main(int argc, char **argv)
{
        forth_obj_t *o;
        FILE *in, *coreo;
        in = fopen_or_fail(startf, "rb");
        if(forth_run(o = forth_init(in, stdout)) < 0)
                return 1;
        fclose(in);
        if(argc > 1) {
                while(++argv, --argc) {
                        forth_seti(o, in = fopen_or_fail(argv[0], "rb"));
                        if(forth_run(o) < 0)
                                return 1;
                        fclose(in);
                }
        } else {
                forth_seti(o, stdin);
                if(forth_run(o) < 0)
                        return 1;
        }
        if(forth_save(o, (coreo = fopen_or_fail(coref, "wb"))) < 0)
                return 1;
        fclose(coreo);
        free(o);
        return 0;
}
