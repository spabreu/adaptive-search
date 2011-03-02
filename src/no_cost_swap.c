/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *			MPI Yves Caniou and Florian Richoux
 *
 *  no_cost_swap.c: wrapper when user function Cost_If_Swap is not defined
 */

#include <stdio.h>

#include "ad_solver.h"

int Cost_If_Swap(int current_cost, int i, int j)
{
  int x;
  int r;

  x = ad_sol[i];
  ad_sol[i] = ad_sol[j];
  ad_sol[j] = x;

  r = Cost_Of_Solution(0);

  ad_sol[j] = ad_sol[i];
  ad_sol[i] = x;

  if (ad_reinit_after_if_swap)
    Cost_Of_Solution(0);

  return r;
}


