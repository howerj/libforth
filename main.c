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
        fio_t in_file = { io_stdin, 0, 0, {NULL} };
        fio_t out_file = { io_stdout, 0, 0, {NULL} };
        fio_t err_file = { io_stderr, 0, 0, {NULL} };

        mw reg[MAX_REG] = { 0 };
        mw dic[MAX_DIC] = { 0 };
        mw var[MAX_VAR] = { 0 };
        mw ret[MAX_RET] = { 0 };
        char str[MAX_STR] = { 0 };

        fobj_t fo;

        in_file.fio = io_rd_file;
        if ((in_file.iou.f = fopen("start.fs", "r")) == NULL) {
                fprintf(stderr, "Unable to open initial input file!\n");
                return 1;
        }

        SM_maxReg = MAX_REG;
        SM_maxDic = MAX_DIC;
        SM_maxVar = MAX_VAR;
        SM_maxRet = MAX_RET;
        SM_maxStr = MAX_STR;
        SM_inputBufLen = 32;
        SM_dictionaryOffset = 4;
        SM_sizeOfMW = sizeof(mw);
        INI = true;

        fo.in_file = &in_file;
        fo.out_file = &out_file;
        fo.err_file = &err_file;
        fo.reg = reg;
        fo.dic = dic;
        fo.var = var;
        fo.ret = ret;
        fo.str = str;

        forth_monitor(&fo);
#ifdef DEBUG_PRN
        debug_print(&fo);
#endif        
        return 0;
}
