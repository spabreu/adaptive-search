/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  costas.c: the Costas Array Problem (2011)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ad_solver.h"


/*-----------*
 * Constants *
 *-----------*/

#define BIG ((unsigned int) -1 >> 1)



/*-------*
 * Types *
 *-------*/

/*------------------*
 * Global variables *
 *------------------*/

static int *sol;		/* copy of p_ad->sol */
static int size;		/* copy of p_ad->size */
static int size2;		/* (size - 1) / 2 */
static int size_sq;		/* size * size */

static int size_bytes;		/* size * sizeof(int) */

static int *nb_occ;		/* nb occurrences of each distance -(size -1)..-1 1..size-1 (0 is unused) */
static int *first;		/* records the indice of a first occurence of a distance */
static int *err;		/* errors on variables */


				/* for reset: */
static int *save_sol;		/* save the sol[] vector */
static int *best_sol;		/* save the best sol[] found in a reset phase */
static int *i_err;		/* indices of erroneous vars */
static int to_add[10];		/* some values to add (circularly) at reset (see init below) */



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
  size2 = (size - 1) / 2;		/* see http://www.costasarrays.org/costasrefs/chang87remark.pdf */
  size_sq = size * size;

  size_bytes = size * sizeof(int);

  if (nb_occ == NULL)
    {
      nb_occ = (int *) malloc(size * 2 * sizeof(int));
      first = (int *) malloc(size * 2 * sizeof(int));
      err = (int *) malloc(size * sizeof(int));
      save_sol = (int *) malloc(size * sizeof(int));
      best_sol = (int *) malloc(size * sizeof(int));
      i_err = (int *) malloc(size * sizeof(int));

      if (nb_occ == NULL || first == NULL || err == NULL || 
	  save_sol == NULL || best_sol == NULL || i_err == NULL)
	{
	  printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }

  /* to_add[]: here are best values (giving most improvements in descending order):
   *
   * 15: 1 2 7 6 8 5 3 4 9 13 10 12 11
   * 16: 1 2 8 7 6 9 14 5 3 4 10 13 11 12
   * 17: 1 2 8 7 9 15 6 3 10 5 4 11 14 12 13
   * 18: 1 2 9 16 8 7 10 3 6 5 11 4 15 12 14 13
   * 19: 1 2 17 9 8 10 7 3 11 6 4 5 16 12 13 15 14 
   *
   * Remark: never N-1 (why ?)
   *
   * thus good values seem (more or less ordered): 
   * 1 2 size/2, size/2-1, size/2+1, size-2, size/2-3, size-3, size/2+3
   */

  /* check declaration of to_add, size must be enough + 1 for 0 at end */
#if 0
  to_add[0] = 1; to_add[1] = 2; to_add[2] = size / 2 - 1; to_add[3] = size / 2; to_add[4] = size - 2;
#elif 1
  to_add[0] = 1; to_add[1] = 2; to_add[2] = size - 2; to_add[3] = size - 3;
#else
  to_add[0] = 1; to_add[1] = 2; to_add[2] = size / 2; to_add[3] = size - 2;
#endif
 

  Ad_Solve(p_ad);
}


#define ERROR  (size_sq - (dist * dist))

#define ErrOn(k)   { err[k] += ERROR; err[k - dist] += ERROR; }


inline int 
Cost(int *err)
{
  int dist = 1;
  int i, first_i;
  int diff, diff_translated;
  int nb;
  int r = 0;

  if (err) 
    memset(err, 0, size * sizeof(int));

  do
    {
      memset(nb_occ, 0,  size * (2 * sizeof(int)));

      i = dist;
      do
	{
	  diff = sol[i - dist] - sol[i];
	  diff_translated = diff + size;
	  nb = ++nb_occ[diff_translated];

	  if (err) 
	    {
	      if (nb == 1) 
		first[diff_translated] = i;
	      else
		{
		  if (nb == 2)
		    {
		      first_i = first[diff_translated];
		      ErrOn(first_i);
		    }

		  ErrOn(i);
		}
	    }

	  if (nb > 1)
	    r += ERROR;
	}
      while(++i < size);
    }
  while(++dist <= size2);

  return r;
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
  return Cost((should_be_recorded) ? err : NULL);
}



/*
 *  COST_ON_VARIABLE
 *
 *  Evaluates the error on a variable.
 */
int
Cost_On_Variable(int i)
{
  return err[i];
}



/*
 *  EXECUTED_SWAP
 *
 *  Records a swap.
 */

void
Executed_Swap(int i1, int i2)
{
  Cost(err);
}



/*
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */

int
Cost_If_Swap(int current_cost, int i1, int i2)
{
  int x;
  int r;

  x = sol[i1];
  sol[i1] = sol[i2];
  sol[i2] = x;

  r = Cost(NULL);

  sol[i2] = sol[i1];
  sol[i1] = x;

  return r;  
}



/*
 *  RESET
 *
 * Performs a reset (returns the new cost or -1 if unknown or some other data are not updated)
 *
 */

int
Reset(int n, AdData *p_ad)
{
  int i, j, k, sz;
  int max = 0, nb_max = 0, imax;
  int cost_to_exit = p_ad->total_cost;
  int best_cost = BIG;
  int cost;

  memcpy(save_sol, sol, size_bytes);

  for(i = 0; i < size; i++)	/* collect most erroneous vars */
    {
      if (err[i] > max)
        {
          max = err[i];
	  i_err[0] = i;
          nb_max = 1;
        } 
      else if (err[i] == max)
	i_err[nb_max++] = i;
    }

  Random_Array_Permut(i_err, nb_max);
  imax = i_err[--nb_max]; /* chose one var random (most often there is only one) - the last and dec nb_max */


  /* A way to reset: try to shift left/right all sub-vectors starting or ending by imax
   *                 need sol[] to be as at entry.
   */

#if 1
  for(k = 0; k < size; k++)
    {
      /* we need a random here to avoid to be trapped in the same "bests" chain (see best_cost) */

      if (Random_Double() < 0.4)
	continue;

      if (imax < k)
	{
	  i = imax;
	  j = k;
	}
      else
	{
	  i = k;
	  j = imax;
	}
      sz = j - i;

      if (sz <= 1)
	continue;

      sz *= sizeof(int);

	/* the following test is not precise (could be different),
	 * we only want to avoid to do both left and right shift for efficiency reasons */

      if (imax < size2)	
	{			/* shift left 1 cell */
	  memcpy(sol + i, save_sol + i + 1, sz);
	  sol[j] = save_sol[i];

	  if ((cost = Cost(NULL)) < cost_to_exit)
	    return -1;		/* -1 because the err[] is not up-to-date */

	  if (cost < best_cost || (cost == best_cost && Random_Double() < 0.2))
	    {
	      best_cost = cost;
	      memcpy(best_sol, sol, size_bytes);
	    }
	} 
      else 
	{			/* shift right 1 cell */
	  memcpy(sol + i + 1, save_sol + i, sz);
	  sol[i] = save_sol[j];

	  if ((cost = Cost(NULL)) < cost_to_exit)
	    return -1;

	  if (cost < best_cost || (cost == best_cost && Random_Double() < 0.2))
	    {
	      best_cost = cost;
	      memcpy(best_sol, sol, size_bytes);
	    }
	}
      /* restore */
      memcpy(sol + i, save_sol + i, sz + sizeof(int));      
    }
#endif


  /* A way to reset: try to add a constant (circularly) to each element.
   *                 does not need sol[] to be as entry (uses save_sol[]). 
   */

#if 1
  for(j = 0; (k = to_add[j]) != 0; j++)
    {
      for(i = 0; i < size; i++)
	if ((sol[i] = save_sol[i] + k) > size)
	  sol[i] -= size;

      if ((cost = Cost(NULL)) < cost_to_exit)
	return -1;      /* -1 because the err[] is not up-to-date */

#if 1
      if (cost < best_cost && Random_Double() < 0.33333333333)
	{
	  best_cost = cost;
	  memcpy(best_sol, sol, size_bytes);
	}
#endif

    }
  //  memcpy(sol, save_sol, size_bytes); // can be needed depending if what follows need inital sol[] 
#endif



  /* A way to reset: try to shift left from the beginning to some erroneous var.
   *                 does not need sol[] to be as entry (uses save_sol[]). 
   */

#if 1

#define NB_OF_ERR_VARS_TO_TRY 3

  int nb_err = nb_max;		/* NB nb_max has been dec (see above) - thus we forget cur "imax" */
  if (nb_err < NB_OF_ERR_VARS_TO_TRY)		/* add other erroneous vars in i_err[] */
    {
      for(i = 0; i < size; i++)
	if (err[i] > 0 && err[i] < max)
	  i_err[nb_err++] = i;
      
      Random_Array_Permut(i_err + nb_max, nb_err - nb_max); /* some randomness on new vars (don't touch max vars) */
    }

  for(k = 0; k < NB_OF_ERR_VARS_TO_TRY; k++)
    {
      imax = i_err[k];

      if (imax == 0 || /*imax == size - 1 ||*/ Random_Double() < 0.33333333333)
	continue;

      memcpy(sol, save_sol + imax, (size - imax) * sizeof(int));
      memcpy(sol + size - imax, save_sol, imax * sizeof(int));

      if ((cost = Cost(NULL)) < cost_to_exit) /* only if it is a var with max error */
	return -1;      /* -1 because the err[] is not up-to-date */

      if (cost < best_cost)
	{
	  best_cost = cost;
	  memcpy(best_sol, sol, size_bytes);
	}
    }

#endif


  /* return the best found solution */

  memcpy(sol, best_sol, size_bytes);

  return -1;      /* -1 because the err[] is not up-to-date */
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

  p_ad->base_value = 1;

  /* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 50;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 1;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 1; //0

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 5;

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 1000000000;

  if (p_ad->restart_max == -1)
    p_ad->restart_max = 0;
}




/*
 *  DISPLAY_SOLUTION
 *
 *  Displays a solution.
 */
void
Display_Solution(AdData *p_ad)
{
  int size = p_ad->size;
  int *sol = p_ad->sol;
  int i, j;

  int len = 4 + ((size * 4) - (1 + 2 * size)) / 2;
  char buff[len + 1];

  sprintf(buff, "%*s", len, "");
  printf("%s", buff);
  for(i = 0; i < size; i++)
    printf("+-");
  printf("+\n%s", buff);
  for(i = 0; i < size; i++)
    {
      for(j = 0; j < size; j++)
	{
	  if (sol[i] - 1 == j)
	    printf("|*");
	  else
	    printf("| ");
	}
      printf("|\n%s", buff);
    }
  for(i = 0; i < size; i++)
    printf("+-");
  printf("+\n");

  printf("sol:");
  for(i = 0; i < size; i++)
    printf("%4d", sol[i]);
  printf("\n");
  printf("----");
  for(i = 0; i < size; i++)
    printf("----");
  printf("\n");


  for(i = 1; i < size; i++)
    {
      printf("%3d:", i);
      for(j = 1; j <= i; j++)
	printf("  ");
 
      for(j = i; j < size; j++)
	printf("%4d", sol[j - i] - sol[j]);

      printf("\n");
    }
}




/*
 *  CHECK_SOLUTION
 *
 *  Checks if the solution is valid.
 */

int
Check_Solution(AdData *p_ad)
{
  int size = p_ad->size;
  int *sol = p_ad->sol;
  int i, j, d;
  int r = 1;


  if (nb_occ == NULL)
    {
      nb_occ = (int *) malloc(size * 2 * sizeof(int));
      if (nb_occ == NULL)
	{
	  printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }

  for(i = 1; i < size; i++)
    {
      memset(nb_occ, 0, size * 2 * sizeof(int));
      for(j = i; j < size; j++)
	{
	  d = sol[j - i] - sol[j];
	  nb_occ[d + size]++;
	}

      for(d = 1; d < 2 * size; d++)
	{
	  int nr = nb_occ[d];
	  if (nr > 1)
	    {
	      int dist = d - size;
	      printf("ERROR at row %d: distance %d appears %d times\n", i, dist, nr);
	      r = 0;
	    }
	}
    }

  return r;
}
