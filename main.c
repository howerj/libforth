#include <stdlib.h>
#include <stdio.h>
#include "forth.h"

static const char *coref = "forth.core";
static const char *start = "# @todo create, does>, execute, words, .s\n \
# @todo change how dump works \n\
: state 8 ! exit \
: ; immediate ' exit , 0 state exit \
: hex 9 ! ; \
: pwd 10 ; \
: r 1 ; \
: h 0 ; \
: here h @ ; \
: [ immediate 0 state ; \
: ] 1 state ; \
: :noname immediate here 2 , ] ; \
: if immediate ' jz , here 0 , ; \
: else immediate ' j , here 0 , swap dup here swap - swap ! ; \
: then immediate dup here swap - swap ! ; \
: begin immediate here ; \
: until immediate ' jz , here - , ; \
: halt 2 >r ; \
: 0= 0 = ; \
: 1+ 1 + ; \
: '\"' 34 ; \
: ')' 41 ; \
: tab 9 emit ; \
: cr 10 emit ; \
: print begin dup 1+ swap @ dup '\"' = if drop exit then emit 0 until ; \
: imprint r> print >r ;  \
: \" immediate key drop ' imprint , begin key dup , '\"' = until ; \
: .( key drop begin key dup ')' = if drop exit then emit 0 until ; \
: dump 32 begin 1 - dup dup 1024 * swap save drop dup 0= until ; \
: 2dup dup >r >r dup r> swap r> ; \
: line dup . tab dup 4 + swap begin dup @ . tab 1+ 2dup = until drop ; \
: list swap begin line cr 2dup < until ; \
: allot here + h ! ; \
# : words pwd @ begin dup . cr @ dup 32 < until ; \n\
# : hide   find 1 - dup @ 32768 | swap ! ; \n\
: :: [ find : , ] ; \
: create :: 2 , here 2 + , ' exit , 0 state ;  \
# .( OK. ) here . cr \n";

static FILE *fopen_or_fail(const char *name, char *mode)
{       FILE *file;
        if(!(file = fopen(name, mode)))
                perror(name), exit(1);
        return file;
}

int main(int argc, char **argv)
{       FILE *in, *coreo; 
        forth_obj_t *o;
        if(!(o = forth_init(stdin, stdout))) return -1;
        forth_sets(o,start);
        if(forth_run(o) < 0) return -1;
        forth_seti(o,stdin);
        if(argc > 1) {
                while(++argv, --argc) {
                        forth_seti(o, in = fopen_or_fail(argv[0], "rb"));
                        if(forth_run(o) < 0)
                                return 1;
                        fclose(in);
                }
        } else {
                if(forth_run(o) < 0)
                        return 1;
        }
        if(forth_coredump(o, (coreo = fopen_or_fail(coref, "wb"))) < 0)
                return 1;
        fclose(coreo);
        free(o);
        return 0;
}
