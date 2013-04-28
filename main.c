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
#define MAX_DIC 2048
#define MAX_VAR 128
#define MAX_RET 128
#define MAX_STR 2048

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

int main(void)
{
        /*setting I/O streams*/
        fio_t in_file = { io_stdin, 0, 0, {NULL} };
        fio_t out_file = { io_stdout, 0, 0, {NULL} };
        fio_t err_file = { io_stderr, 0, 0, {NULL} };

        fobj_t fo;

        /*initialize input file, fclose is handled elsewhere*/
        in_file.fio = io_rd_file;
        if ((in_file.iou.f = fopen("start.fs", "r")) == NULL) {
                fprintf(stderr, "Unable to open initial input file!\n");
                return 1;
        }

        /*memories of the interpreter*/
        fo.reg = calloc(sizeof(mw), MAX_REG);
        fo.dic = calloc(sizeof(mw), MAX_DIC);
        fo.var = calloc(sizeof(mw), MAX_VAR);
        fo.ret = calloc(sizeof(mw), MAX_RET);
        fo.str = calloc(sizeof(char), MAX_STR);

        /*initializing memory*/
        fo.reg[ENUM_maxReg] = MAX_REG;
        fo.reg[ENUM_maxDic] = MAX_DIC;
        fo.reg[ENUM_maxVar] = MAX_VAR;
        fo.reg[ENUM_maxRet] = MAX_RET;
        fo.reg[ENUM_maxStr] = MAX_STR;
        fo.reg[ENUM_inputBufLen] = 32;
        fo.reg[ENUM_dictionaryOffset] = 4;
        fo.reg[ENUM_sizeOfMW] = sizeof(mw);
        fo.reg[ENUM_INI] = true;

        fo.in_file = &in_file;
        fo.out_file = &out_file;
        fo.err_file = &err_file;

        forth_monitor(&fo);
#ifdef DEBUG_PRN
        debug_print(&fo);
#endif
        free(fo.reg);
        free(fo.dic);
        free(fo.var);
        free(fo.ret);
        free(fo.str);
        return 0;
}
