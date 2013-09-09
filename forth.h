/* 
 * Richard James Howe
 * Howe Forth.
 *
 * Configuration file and interface API
 *
 * @author         Richard James Howe.
 * @copyright      Copyright 2013 Richard James Howe.
 * @license        LGPL      
 * @email          howe.r.j.89@gmail.com
 *
 */

/*#define RUN4X*/

#ifndef forth_h_header_guard
#define forth_h_header_guard

#define MAX_PRN_STR    64
#define MAX_ERR_STR    64
#define MAX_STRLEN     32
#define MAX_INSTRM     32

/*minimum memory requirements*/
#define MIN_REG        32
#define MIN_DIC        512
#define MIN_RET        32
#define MIN_VAR        32
#define MIN_STR        512
#define MIN_INBUF      16
#define MIN_DIC_OFF    4

/* Enums */
enum bool { false, true };

enum forth_io {
        io_stdin,               /*read from stdin */
        io_stdout,              /*write to stdout */
        io_stderr,              /*output to stderr */
        io_wr_file,             /*write to file */
        io_rd_file,             /*read from file */
        io_wr_str,              /*write to a string */
        io_rd_str               /*read from a string (null terminated!) */
};

enum forth_primitives {
        PUSH_INT, COMPILE, RUN, DEFINE, IMMEDIATE, READ, COMMENT, EXIT,
        BRANCH, NBRANCH, PLUS, MINUS, MUL, MOD, DIV,
        LS, RS, AND, OR, INV, XOR, INC, DEC, EQ, LESS, MORE,
        FETCH_REG, FETCH_DIC, PICK, FETCH_STR,
        STORE_REG, STORE_DIC, STORE_VAR, STORE_STR,
        KEY, EMIT, DUP, DROP, SWAP, OVER, TOR, FROMR,
        TAIL, QUOTE, COMMA, PRINTNUM, GET_WORD, STRLEN, ISNUMBER, STRNEQU, FIND,
        EXECUTE,
        KERNEL,
        ERROR,
        LAST_PRIMITIVE
};

/*forth_interpreter() return calls*/
enum forth_syscall {
        SYS_RESET,
        SYS_FOPEN, SYS_FCLOSE, SYS_FLUSH,
        SYS_REMOVE, SYS_RENAME, SYS_REWIND
            /* remove() rename() tmpfile() tmpnam() ... other stdio.h functions */
};

/*Options passed to system call*/
enum forth_syscall_options {
        SYS_OPT_IN, SYS_OPT_OUT, SYS_OPT_ERR
};

#define FORTH_ERROR_XMACRO \
  X(ERR_OK, "OK!\n")\
  X(ERR_FAILURE, "General Failure? Unknown cause.\n")\
  X(ERR_REG, "Err: Register addr.\n")\
  X(ERR_DIC, "Err: Dictionary addr.\n" )\
  X(ERR_VAR,"Err: Variable addr.\n" )\
  X(ERR_RET, "Err: Return addr.\n")\
  X(ERR_STR, "Err: String addr.\n")\
  X(ERR_PWD, "Err: PWD!\n")\
  X(ERR_NEXT, "Err: NEXT!\n")\
  X(ERR_PC,"Err: PC!\n" )\
  X(ERR_TOS_REG, "Err: TOS addr. Reg.\n")\
  X(ERR_TOS_DIC, "Err: TOS addr. Dic.\n")\
  X(ERR_TOS_VAR, "Err: TOS addr. Var.\n")\
  X(ERR_TOS_RET, "Err: TOS addr. Ret.\n")\
  X(ERR_TOS_STR, "Err: TOS addr. Str.\n")\
  X(ERR_OP0, "Err: OP0!\n")\
  X(ERR_OP1, "Err: OP1!\n")\
  X(ERR_DIV0, "Err: Division by zero.\n")\
  X(ERR_MOD0, "Err: Modulo by zero.\n")\
  X(ERR_IO, "Err: IO Error.\n")\
  X(ERR_EOF, "EOF\n")\
  X(ERR_BASE, "Err: Base.\n")\
  X(ERR_INSTRUCTION, "Err: Illegal Instruction.\n")\
  X(ERR_WORD, "Err: Word not found?\n")\
  X(ERR_ABNORMAL_END, "Fatal Err: !!!Abnormal End!!!\n")\
  X(ERR_GENERAL, "General Error.\n")\
  X(ERR_SYSCALL, "System call failed.\n")\
  X(ERR_SYSCALL_OPTIONS, "Not a system call option.\n")\
  X(ERR_NOTSYSCALL, "Err: Not a system call\n")\
  X(ERR_MINIMUM_MEM, "Fatal Err: Minimum memory requirements not met\n")\
  X(ERR_CYCLES, "Cycles complete\n" )\
  X(HALT,"HALTING FORTH\n" )\
  X(LAST_SYS, "Fatal Err: Incorrect error code or call!\n")

#define X(a, b) a,
enum forth_errors {
  FORTH_ERROR_XMACRO
};
#undef X 

enum forth_registers {
        ENUM_NEXT,
        ENUM_PC,
        ENUM_TOS,
        ENUM_RET,
        ENUM_VAR,
        ENUM_DIC,
        ENUM_STR,
        ENUM_PWD,
        ENUM_OP0,
        ENUM_OP1,
        ENUM_A,
        ENUM_B,
        ENUM_C,
        ENUM_CPF,
        ENUM_EXF,
        ENUM_INI,
        ENUM_maxReg,
        ENUM_maxDic,
        ENUM_maxRet,
        ENUM_maxVar,
        ENUM_maxStr,
        ENUM_inputBufLen,
        ENUM_dictionaryOffset,
        ENUM_sizeOfMW,
        ENUM_cycles,
        ENUM_ccount,
        ENUM_inStrm
};

/*vm macros*/
#define NEXT    reg[ENUM_NEXT]
#define PC      reg[ENUM_PC]
#define TOS     reg[ENUM_TOS]
#define RET     reg[ENUM_RET]

#define VAR     reg[ENUM_VAR]
#define DIC     reg[ENUM_DIC]
#define STR     reg[ENUM_STR]
#define PWD     reg[ENUM_PWD]

#define OP0     reg[ENUM_OP0]
#define OP1     reg[ENUM_OP1]
#define A       reg[ENUM_A]
#define B       reg[ENUM_B]

#define C       reg[ENUM_C]
#define CPF     reg[ENUM_CPF]
#define EXF     reg[ENUM_EXF]
#define INI     reg[ENUM_INI]

#define SM_maxReg  reg[ENUM_maxReg]
#define SM_maxDic  reg[ENUM_maxDic]
#define SM_maxRet  reg[ENUM_maxRet]
#define SM_maxVar  reg[ENUM_maxVar]

#define SM_maxStr  reg[ENUM_maxStr]
#define SM_inputBufLen  reg[ENUM_inputBufLen]
#define SM_dictionaryOffset  reg[ENUM_dictionaryOffset]
#define SM_sizeOfMW  reg[ENUM_sizeOfMW]

#define CYCLES  reg[ENUM_cycles]
#define CCOUNT  reg[ENUM_ccount]

#define IN_STRM reg[ENUM_inStrm]

typedef signed int mw;

/*if input or output is a file or string, store point to it*/
union io_u {
        FILE *f;
        char *s;
};

/*IO redirections.*/
struct fio_s {
        enum forth_io fio;
        mw str_index;           /*index into string */
        mw str_max_len;         /*max string length */
        union io_u iou;
};

typedef struct fio_s fio_t;

struct forth_obj {
        fio_t *in_file[MAX_INSTRM];     /*File input redirection */
        fio_t *out_file;        /*File output redirection */
        fio_t *err_file;        /*File error output redirection */
        mw *reg;                /*pointer to registers */
        mw *dic;                /*pointer to dictionary */
        mw *var;                /*pointer to variable stack */
        mw *ret;                /*pointer to return stack */
        char *str;              /*pointer to character storage */
};

typedef struct forth_obj fobj_t;

/* Function prototypes for external API */
mw forth_interpreter(fobj_t * fo);
mw forth_monitor(fobj_t * fo);
#endif                          /*end header guard */
