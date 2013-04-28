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
#include <stdlib.h>

#define MAX_REG 32
#define MAX_DIC 4196
#define MAX_VAR 256
#define MAX_RET 256
#define MAX_STR 4196

/*Define me via the command line.*/
#ifdef DEBUG_PRN
/*print out a table of integers*/
void print_table(mw * p, int len, FILE * f)
{
        int i;
        for (i = 0; i < len; i++)
                if (i % 4 == 0)
                        fprintf(f, "\n%08X:\t\t%08X\t\t", i, p[i]);
                else
                        fprintf(f, "%08X\t\t", p[i]);

        fprintf(f, "\n");
}

/*print out a character table*/
void print_char_table(char *p, int len, FILE * f)
{
        int i;
        for (i = 0; i < len; i++)
                if (i % 4 == 0) {
                        if (p[i] != '\0')
                                fprintf(f, "\n%08X:\t\t%c\t\t", i, p[i]);
                        else
                                fprintf(f, "\n%08X:\t\t' '\t\t", i);
                } else {
                        if (p[i] != '\0')
                                fprintf(f, "%c\t\t", p[i]);
                        else
                                fprintf(f, "' '\t\t");
                }
        fprintf(f, "\n");
}

/*print out main memory.*/
void debug_print(fobj_t *fo){

        FILE *table_out;
        if ((table_out = fopen("memory.txt", "w")) == NULL) {
                printf("Unable to open log file!\n");
                return;
        }

        fprintf(table_out, "Registers:\n");
        print_table(fo->reg, MAX_REG, table_out);
        fprintf(table_out, "Dictionary:\n");
        print_table(fo->dic, MAX_DIC, table_out);
        fprintf(table_out, "Variable stack:\n");
        print_table(fo->var, MAX_VAR, table_out);
        fprintf(table_out, "Return stack:\n");
        print_table(fo->ret, MAX_RET, table_out);
        fprintf(table_out, "String storage:\n");
        print_char_table(fo->str, MAX_STR, table_out);

        fflush(table_out);
        fclose(table_out);

        fprintf(stderr, "Maximum memory usuage = %d\n",
                (sizeof(mw) * (MAX_REG + MAX_DIC + MAX_VAR + MAX_RET)) +
                (sizeof(char) * MAX_STR));
}
#endif

#define CALLOC_FAIL(X)\
      if((X)==NULL){\
          fprintf(stderr,"calloc() failed <%s:%d>\n", __FILE__,__LINE__);\
          return NULL;\
      }
fobj_t *forth_vm_create(mw reg_l, mw dic_l, mw var_l, mw ret_l, mw str_l){
        /*the vm forth object*/
        fobj_t *fo = calloc(1,sizeof(fobj_t));
        CALLOC_FAIL(fo);

         /*setting I/O streams*/
        fio_t *in_file  = calloc(1,sizeof(fio_t));
        fio_t *out_file = calloc(1,sizeof(fio_t));
        fio_t *err_file = calloc(1,sizeof(fio_t));
        CALLOC_FAIL(in_file);
        CALLOC_FAIL(out_file);
        CALLOC_FAIL(err_file);

        in_file->fio  = io_stdin;
        out_file->fio = io_stdout;
        err_file->fio = io_stderr;
        
        fo->in_file  = in_file;
        fo->out_file = out_file;
        fo->err_file = err_file;

        /*memories of the interpreter*/
        fo->reg = calloc(reg_l,sizeof(mw));
        fo->dic = calloc(dic_l,sizeof(mw));
        fo->var = calloc(var_l,sizeof(mw));
        fo->ret = calloc(ret_l,sizeof(mw));
        fo->str = calloc(str_l,sizeof(char));
        CALLOC_FAIL(fo->reg);
        CALLOC_FAIL(fo->dic);
        CALLOC_FAIL(fo->var);
        CALLOC_FAIL(fo->ret);
        CALLOC_FAIL(fo->str);

        /*initialize input file, fclose is handled elsewhere*/
        fo->in_file->fio = io_rd_file;
        if ((fo->in_file->iou.f = fopen("start.fs", "r")) == NULL) {
                fprintf(stderr, "Unable to open initial input file!\n");
                return NULL;
        }

        /*initializing memory*/
        fo->reg[ENUM_maxReg] = MAX_REG;
        fo->reg[ENUM_maxDic] = MAX_DIC;
        fo->reg[ENUM_maxVar] = MAX_VAR;
        fo->reg[ENUM_maxRet] = MAX_RET;
        fo->reg[ENUM_maxStr] = MAX_STR;
        fo->reg[ENUM_inputBufLen] = 32;
        fo->reg[ENUM_dictionaryOffset] = 4;
        fo->reg[ENUM_sizeOfMW] = sizeof(mw);
        fo->reg[ENUM_INI] = true;

        printf("Forth object initialized.\n");
        return fo;
}

int main(void)
{
        fobj_t *fo;

        fo = forth_vm_create(MAX_REG,MAX_DIC,MAX_VAR,MAX_RET,MAX_STR);

        forth_monitor(fo);
#ifdef DEBUG_PRN
        debug_print(fo);
#endif
        free(fo->reg);
        free(fo->dic);
        free(fo->var);
        free(fo->ret);
        free(fo->str);
        free(fo->in_file);
        free(fo->out_file);
        free(fo->err_file);
        free(fo);
        return 0;
}
