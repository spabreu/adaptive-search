/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  interval.c: the all-interval series problem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad_solver.h"


#if 0
#define SLOW
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

static int *nb_occ;		/* nb occurrences (to compute total cost) 0 is unused */




/*------------*
 * Prototypes *
 *------------*/


/*
 *  MODELING
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

  if (nb_occ == NULL)
    {
      nb_occ = (int *) malloc(size * sizeof(int));

      if (nb_occ == NULL)
	{
	  printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }

  Ad_Solve(p_ad);
}


/*
 *  COST
 *
 *  Computes a cost associated to the array of occurrences.
 */

static int
Cost(int nb_occ[])

{
#ifndef SLOW

  int i = size;

  nb_occ[0] = 0;                /* 0 is unused, use it as a sentinel */

  while(nb_occ[--i])
    ;

  return i;

#else  // less efficient (use it with -p 5 -f 4 -l 2 -P 80)

  int r = 0, i;

  for(i = 1; i < size; i++)
    if (nb_occ[i] == 0)
      r += i;

  return r;

#endif
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

  memset(nb_occ, 0, size * sizeof(int));
  
  for(i = 0; i < size - 1; i++)
    nb_occ[abs(sol[i] - sol[i + 1])]++;

  return Cost(nb_occ);
}




/*
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */

int
Cost_If_Swap(int current_cost, int i1, int i2)
{
  int s1, s2;
  int rem1, rem2, rem3, rem4;
  int add1, add2, add3, add4;
				/* we know i1 < i2 due to ad.exhaustive */
				/* else uncomment this */
#if 0
  if (i1 > i2)
    {
      i = i1;
      i1 = i2;
      i2 = i;
    }
#endif

  s1 = sol[i1];
  s2 = sol[i2];

  if (i1 > 0)
    {
      rem1 = abs(sol[i1 - 1] - s1); nb_occ[rem1]--; 
      add1 = abs(sol[i1 - 1] - s2); nb_occ[add1]++; 
    }
  else
    rem1 = add1 = 0;


  if (i1 < i2 - 1)		/* i1 and i2 are not consecutive */
    {
      rem2 = abs(s1 - sol[i1 + 1]); nb_occ[rem2]--; 
      add2 = abs(s2 - sol[i1 + 1]); nb_occ[add2]++; 

      rem3 = abs(sol[i2 - 1] - s2); nb_occ[rem3]--; 
      add3 = abs(sol[i2 - 1] - s1); nb_occ[add3]++; 
    }
  else
    rem2 = add2 = rem3 = add3 = 0;

  if (i2 < size - 1)
    {
      rem4 = abs(s2 - sol[i2 + 1]); nb_occ[rem4]--;
      add4 = abs(s1 - sol[i2 + 1]); nb_occ[add4]++;
    }
  else
    rem4 = add4 = 0;

  int r = Cost(nb_occ);

  /* undo */

  nb_occ[rem1]++; nb_occ[rem2]++; nb_occ[rem3]++; nb_occ[rem4]++; 
  nb_occ[add1]--; nb_occ[add2]--; nb_occ[add3]--; nb_occ[add4]--;

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
  int s1, s2;
  int rem1, rem2, rem3, rem4;
  int add1, add2, add3, add4;

				/* we know i1 < i2 due to ad.exhaustive */
				/* else uncomment this */
#if 0
  if (i1 > i2)
    {
      int i = i1;
      i1 = i2;
      i2 = i;
    }
#endif

  s1 = sol[i2];			/* swap already executed */
  s2 = sol[i1];

  if (i1 > 0)
    {
      rem1 = abs(sol[i1 - 1] - s1); nb_occ[rem1]--; 
      add1 = abs(sol[i1 - 1] - s2); nb_occ[add1]++; 
    }


  if (i1 < i2 - 1)              /* i1 and i2 are not consecutive */
    {
      rem2 = abs(s1 - sol[i1 + 1]); nb_occ[rem2]--; 
      add2 = abs(s2 - sol[i1 + 1]); nb_occ[add2]++; 

      rem3 = abs(sol[i2 - 1] - s2); nb_occ[rem3]--; 
      add3 = abs(sol[i2 - 1] - s1); nb_occ[add3]++; 
    }

  if (i2 < size - 1)
    {
      rem4 = abs(s2 - sol[i2 + 1]); nb_occ[rem4]--;
      add4 = abs(s1 - sol[i2 + 1]); nb_occ[add4]++;
    }
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
  p_ad->size = p_ad->param;

#ifndef SLOW
  p_ad->first_best = 1;
#endif

				/* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 66;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 1;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 25;

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 10000000;

  if (p_ad->restart_max == -1)
    p_ad->restart_max = 0;
}




/*
 *  CHECK_SOLUTION
 *
 *  Checks if the solution is valid.
 */

int
Check_Solution(AdData *p_ad)
{
  int r = 1;
  int i;


  if (nb_occ == NULL)
    {
      nb_occ = (int *) malloc(size * sizeof(int));
      if (nb_occ == NULL)
	{
	  printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }

  memset(nb_occ, 0, p_ad->size * sizeof(int));

  for(i = 0; i < p_ad->size - 1; i++)
    nb_occ[abs(p_ad->sol[i] - p_ad->sol[i + 1])]++;

  for(i = 1; i < p_ad->size; i++)
    if (nb_occ[i] > 1)
      {
	printf("ERROR distance %d appears %d times\n", i, nb_occ[i]);
	r = 0;
      }

  return r;
}
