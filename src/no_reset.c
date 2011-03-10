/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  no_reset.c: wrapper when user function Reset is not defined
 */

#include <stdio.h>

#include "ad_solver.h"

/*
 *  RESET
 *
 * Performs a reset (returns the new cost or -1 if unknown)
 */
int
Reset(int n, AdData *p_ad)
{
  int i, j, x;
  int size = p_ad->size;
  int *sol = p_ad->sol;

  while(n--)
    {
      i = Random(size);
      j = Random(size);

      p_ad->nb_swap++;

      x = sol[i];
      sol[i] = sol[j];
      sol[j] = x;

#if UNMARK_AT_RESET == 1
      Ad_Un_Mark(i);
      Ad_Un_Mark(j);
#endif
    }

  return -1;
}


