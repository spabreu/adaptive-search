/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  no_cost_var.c: wrapper when user function Cost_On_Variable is not defined
 */

#include <stdio.h>

#include "ad_solver.h"

int
Cost_On_Variable(int k)
{
  fprintf(stderr, "%s:%d: error: wrapper Cost_On_Variable function called\n",
	  __FILE__, __LINE__);
  return 0;
}


static void
Init(void) __attribute__ ((constructor));

static void
Init(void)
{
  ad_no_cost_var_fct = 1;
}
