/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  partit.c: the partition problem
 */

#include <stdio.h>
#include <stdlib.h>

#include "ad_solver.h"



/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/

/*------------------*
 * Global variables *
 *------------------*/

static int size;		/* copy of p_ad->size */
static int *sol;		/* copy of p_ad->sol */

static int size2;		/* size / 2 */

static int coeff;
static int sum_mid_x, cur_mid_x;
static long long sum_mid_x2, cur_mid_x2;

/*------------*
 * Prototypes *
 *------------*/


/*
 *  MODELING
 *
 *  NB: partit 32 and 48 are often very difficult, 
 *  we then use a restart limit (e.g. at 100 iters do a restart)
 *  with theses restarts it works well.
 */



/*
 *  SOLVE
 *
 *  Initializations needed for the resolution.
 */

void
Solve(AdData *p_ad)
{
  sol = p_ad->sol;
  size = p_ad->size;

  size2 = size / 2;

  sum_mid_x = p_ad->data32[0];
  coeff = p_ad->data32[1];
  sum_mid_x2 = p_ad->data64[0];

  Ad_Solve(p_ad);
}




/*
 *  COST_OF_SOLUTION
 *
 *  Returns the total cost of the current solution.
 *  Also computes errors on constraints for subsequent calls to
 *  Cost_On_Variable, Cost_If_Swap and Executed_Swap.
 */

int
Cost_Of_Solution(int should_be_recorded)
{
  int i;
  int r;
  int x;

  cur_mid_x = cur_mid_x2 = 0;
  for(i = 0; i < size2; i++)
    {
      x = sol[i];
      cur_mid_x += x;
      cur_mid_x2 += x * x;
    }

  r = coeff * abs(sum_mid_x - cur_mid_x) + abs(sum_mid_x2 - cur_mid_x2);

  return r;
}


/*
 *  NEXT_I and NEXT_J
 *
 *  Return the next pair i/j to try (for the exhaustive search).
 */

int Next_I(int i)
{
  i++;
  return i < size2 ? i : size;
}

int Next_J(int i, int j)
{
  return (j < 0) ? size2 : j + 1;
}




/*
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */

int
Cost_If_Swap(int current_cost, int i1, int i2)
{
  int xi1, xi12, xi2, xi22, cm_x, cm_x2, r;

#if 0				/* useless with customized Next_I and Next_J */
  if (i1 >= size2 || i2 < size2)
    return (unsigned) -1 >> 1;
#endif

  xi1 = sol[i1];
  xi2 = sol[i2];

  xi12 = xi1 * xi1;
  xi22 = xi2 * xi2;

  cm_x = cur_mid_x - xi1 + xi2;
  cm_x2 = cur_mid_x2 - xi12 + xi22;
  r = coeff * abs(sum_mid_x - cm_x) + abs(sum_mid_x2 - cm_x2);

  return r;
}




/*
 *  EXECUTED_SWAP
 *
 *  Records a swap.
 */

void
Executed_Swap(int i1, int i2)
{
  int xi1, xi12, xi2, xi22;

  xi1 = sol[i2];		/* swap already executed */
  xi2 = sol[i1];

  xi12 = xi1 * xi1;
  xi22 = xi2 * xi2;

  cur_mid_x = cur_mid_x - xi1 + xi2;
  cur_mid_x2 = cur_mid_x2 - xi12 + xi22;
}








int param_needed = 1;		/* overwrite var of main.c */

/*
 *  INIT_PARAMETERS
 *
 *  Initialization function.
 */

void
Init_Parameters(AdData *p_ad)
{
  int size = p_ad->param;

  p_ad->size = size;

  if (size < 8 || size % 4 != 0)
    {
      printf("no solution with size = %d\n", size);
      exit(1);
    }

/*  The sums are as follows:
 *  sum_x = size * (size + 1) / 2
 *  sum_x2 = size * (size + 1) * (2 * size + 1) / 6
 *
 *  We are interested in theses sums / 2 thus:
 */

  int sum_mid_x = (size * (size + 1)) / 4;
  long long sum_mid_x2 = ((long long) sum_mid_x * (2 * size + 1)) / 3LL;
  int coeff = sum_mid_x2 / sum_mid_x;

  printf("mid sum x = %d,  mid sum x^2 = %lld, coeff: %d\n",
	 sum_mid_x, sum_mid_x2, coeff);

  p_ad->data32[0] = sum_mid_x;
  p_ad->data32[1] = coeff;
  p_ad->data64[0] = sum_mid_x2;


  p_ad->base_value = 1;
  p_ad->break_nl = size / 2;

  /* defaults */

  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 80;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 1;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 1;

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 100; // (size < 100) ? 10 : (size < 1000) ? 150 : size / 10;

  if (p_ad->restart_max == -1)
    p_ad->restart_max = 100000;
}




/*
 *  CHECK_SOLUTION
 *
 *  Checks if the solution is valid.
 */

int
Check_Solution(AdData *p_ad)
{
  int i;
  int size = p_ad->size;
  int size2 = size / 2;
  int sum_a = 0, sum_b = 0;
  long long sum_a2 = 0, sum_b2 = 0;

  for(i = 0; i < size2; i++)
    {
      sum_a += p_ad->sol[i];
      sum_a2 += p_ad->sol[i] * p_ad->sol[i];
    }

  for(; i < size; i++)
    {
      sum_b += p_ad->sol[i];
      sum_b2 += p_ad->sol[i] * p_ad->sol[i];
    }

  if (sum_a != sum_b)
    {
      printf("ERROR sum a: %d != sum b: %d\n", sum_a, sum_b);
      return 0;
    }

  if (sum_a2 != sum_b2)
    {
      printf("ERROR sum a^2: %lld != sum b: %lld\n", sum_a2, sum_b2);
      return 0;
    }

  return 1;
}
