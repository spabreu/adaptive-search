/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  
 *  ChechCCompilerInline.c: adapted from DIET project
 */

#ifndef __cplusplus
// Check wether compiler accepts inlining for both functions and static
// functions and for which keyword among inline, __inline__, __inline
typedef int foo_t;
static POSSIBLE_INLINE_KEYWORD foo_t static_foo () { return 0; }
       POSSIBLE_INLINE_KEYWORD foo_t        foo () { return 0; }
int main()
{
   (void)static_foo();
   (void)foo();
}
#endif

