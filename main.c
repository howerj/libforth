/*
 * Howe Forth: Wrapper/Top Level.
 * @author         Richard James Howe.
 * @copyright      Copyright 2013 Richard James Howe.
 * @license        LGPL      
 * @email          howe.r.j.89@gmail.com
 *
 */

#include <stdio.h>
#include "forth.h"
#include "hhforth.h"

#define MAX_REG 32
#define MAX_DIC (1024*1024)
#define MAX_VAR 8192
#define MAX_RET 8192
#define MAX_STR (1024*1024)

int main(void)
{
        fobj_t *fo;

        fprintf(stderr, "HOWE FORTH [%s:%s]:\n\tSTARTING.\n", __DATE__,
                __TIME__);
        fo = forth_obj_create(MAX_REG, MAX_DIC, MAX_VAR, MAX_RET, MAX_STR);
        CALLOC_FAIL(fo, -1);    /*memory might not be free()'d on error. */
        fprintf(stderr, "\tRUNNING.\n");
        fprintf(stderr, "\t[RETURNED:%X]\n", (unsigned int)forth_monitor(fo));
#ifdef DEBUG_PRN
        fprintf(stderr, "\tDEBUG PRINT RUNNING\n");
        debug_print(fo);
#endif
        forth_obj_destroy(fo);
        return 0;
}
