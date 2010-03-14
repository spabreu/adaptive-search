/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  no_displ_sol.c: wrapper when user function Display_Solution is not defined
 */

#include <stdio.h>

#include "ad_solver.h"

/*
 *  DISPLAY_SOLUTION
 *
 */
void
Display_Solution(AdData *p_ad)
{
  Ad_Display(p_ad->sol, p_ad, NULL);
}

static void
Init(void) __attribute__ ((constructor));

static void
Init(void)
{
  ad_no_displ_sol_fct = 1;
}
