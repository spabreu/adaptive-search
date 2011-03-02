/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *			MPI Yves Caniou and Florian Richoux
 *
 *  langford.c: the Langford's problem
 */

#include <stdio.h>
#include <stdlib.h>

#include "ad_solver.h"

#if 0
# define SLOW
#endif

#ifdef MPI
# include <sys/time.h>
#endif

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
static int order;		/* size / 2 */


/*------------*
 * Prototypes *
 *------------*/


/*
 *  MODELING
 *
 *  order = n of the problem
 *  size  = order * 2 since we solve Langford(2, order)
 *  sol[] = values: sol[i] = j (j in 0..size-1) means:
 *             if i < order then 1st occurrence of i+1 appears in position j
 *                          else 2nd occurrence of i-order+1 appears in pos j
 *
 *             for instance, a solution for L(2,3) is:
 *                 i   0 1 2 3 4 5
 *             sol[i]: 2 0 5 4 3 1 which represents the sequence:
 *                     2 3 1 2 1 3
 */


/**
 *  SOLVE
 *
 *  Initializations needed for the resolution.
 */
void Solve(AdData *p_ad)
{
  sol = p_ad->sol;
  size = p_ad->size;
  order = p_ad->size / 2;

  Ad_Solve(p_ad);
}


static int Cost_Var(int i)	/* here i < order */
{				
  int r = 0;
  int x, y, between;

  x = sol[i];
  y = sol[order + i];

  between = abs(x - y) - 1;

#ifndef SLOW
  r = abs(between - (i + 1));
  r = r * r;
#else
  if (between != i + 1)
    r = (i + 1) * (i + 1);
#endif

  return r;
}


/**
 *  COST_OF_SOLUTION
 *
 *  Returns the total cost of the current solution.
 *  Also computes errors on constraints for subsequent calls to
 *  Cost_On_Variable, Cost_If_Swap and Executed_Swap.
 */
int Cost_Of_Solution(int should_be_recorded)
{
  int i;
  int r = 0;

  for(i = 0; i < order; i++)
    r += Cost_Var(i);

  return r;
}


/**
 *  COST_ON_VARIABLE
 *
 *  Evaluates the error on a variable.
 */
int Cost_On_Variable(int i)
{
  if (i >= order)
    i -= order;

  return Cost_Var(i);
}


int param_needed = 1;		/* overwrite var of main.c */
/**
 *  INIT_PARAMETERS
 *
 *  Initialization function.
 */
void Init_Parameters(AdData *p_ad)
{
#ifdef MPI
# ifdef YC_DEBUG
  struct timeval tv ;
  gettimeofday(&tv, NULL);
  printf("%d begins %ld:%ld\n", my_num, (long int)tv.tv_sec,
	  (long int)tv.tv_usec) ;
# endif
#endif

  int order = p_ad->param;

  p_ad->size = order * 2;

  if (order % 4 != 0 && order % 4 != 3)
    {
      printf("no solution with size = %d\n", order);
      exit(1);
    }
  /* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 4;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 1;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

#ifndef SLOW
  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 3;
#else
  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 0;
#endif

  if (p_ad->reset_percent == -1)
#if 1
    p_ad->nb_var_to_reset = (order % 2 == 0) ? 2 : 1; // 2 if even / 1 if odd
#else
  p_ad->reset_percent = (order % 2 == 0) ? 2 : 3; // 2% if even / 3% if odd
#endif

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 10000000;

  if (p_ad->restart_max == -1)
    p_ad->restart_max = 0;
}


/**
 *  DISPLAY_SOLUTION
 *
 *  Displays a solution.
 */
void Display_Solution(AdData *p_ad)
{
  int i, j;
  int order = p_ad->size / 2;

  //  Ad_Display(p_ad->sol, p_ad, NULL); // to see actual values
  for(i = 0; i < p_ad->size; i++)
    {
      for(j = 0; p_ad->sol[j] != i; j++)
	;
      if (j >= order)
	j -= order;
      printf("%d ", j + 1);
    }
  printf("\n");
}


/**
 *  CHECK_SOLUTION
 *
 *  Checks if the solution is valid.
 */
int Check_Solution(AdData *p_ad)
{
  int order = p_ad->size / 2;
  int *sol = p_ad->sol;
  int i;
  int x, y, between;

  for(i = 0; i < order; i++)
    {
      x = sol[i];
      y = sol[order + i];
      between = abs(x - y) - 1;

      if (between != i + 1)
	{
	  printf("ERROR between the two %d there are %d values !\n", i + 1, between);
	  return 0;
	}
    }

  return 1;
}
