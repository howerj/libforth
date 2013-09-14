/*
 * Howe Forth: Hosted Howe Forth, Header
 * @author         Richard James Howe.
 * @copyright      Copyright 2013 Richard James Howe.
 * @license        LGPL      
 * @email          howe.r.j.89@gmail.com
 *
 */

#ifndef hhforth_h_header_guard    /* begin header guard for hhforth.h */
#define hhforth_h_header_guard

void debug_print(fobj_t * fo);
fobj_t *forth_obj_create(mw reg_l, mw dic_l, mw var_l, mw ret_l, mw str_l);
void forth_obj_destroy(fobj_t * fo);

#define CALLOC_FAIL(X,RET)\
      if((X)==NULL){\
          fprintf(stderr,"calloc() failed <%s:%d>\n", __FILE__,__LINE__);\
          return (RET);\
      }

#endif 
