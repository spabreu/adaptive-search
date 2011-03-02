/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *			MPI Yves Caniou and Florian Richoux
 *
 *  cap.c: the Costas Array Problem (2011)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ad_solver.h"

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

static int *sol;		/* copy of p_ad->sol */
static int size;		/* copy of p_ad->size */
static int size2;		/* (size - 1) / 2 */
static int size_sq;		/* size * size */
static int *nb_occ;		/* nb occurrences of each distance -(size -1)..-1 1..size-1 (0 is unused) */
static int *first;		/* records the indice of a first occurence of a distance */
static int *err;		/* errors on variables */


/*------------*
 * Prototypes *
 *------------*/

/*
 *  MODELING
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
  size2 = (size - 1) / 2;		/* see http://www.costasarrays.org/costasrefs/chang87remark.pdf */
  size_sq = size * size;

  if (nb_occ == NULL)
    {
      nb_occ = (int *) malloc(size * 2 * sizeof(int));
      first = (int *) malloc(size * 2 * sizeof(int));
      err = (int *) malloc(size * sizeof(int));

      if (nb_occ == NULL || first == NULL || err == NULL)
	{
	  printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }

  Ad_Solve(p_ad);
}


#define ERROR  size_sq - (dist * dist)
#define ErrOn(k)   { err[k] += ERROR; err[k - dist] += ERROR; }
inline int Cost(int *err)
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


/**
 *  COST_OF_SOLUTION
 *
 *  Returns the total cost of the current solution.
 *  Also computes errors on constraints for subsequent calls to
 *  Cost_On_Variable, Cost_If_Swap and Executed_Swap.
 */
int Cost_Of_Solution(int should_be_recorded)
{
  return Cost((should_be_recorded) ? err : NULL);
}


/**
 *  COST_ON_VARIABLE
 *
 *  Evaluates the error on a variable.
 */
int Cost_On_Variable(int i)
{
  return err[i];
}


/**
 *  EXECUTED_SWAP
 *
 *  Records a swap.
 */
void Executed_Swap(int i1, int i2)
{
  Cost(err);
}


/**
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */
int Cost_If_Swap(int current_cost, int i1, int i2)
{
  int x = sol[i1];
  int r;

  x = sol[i1];
  sol[i1] = sol[i2];
  sol[i2] = x;

  r = Cost(NULL);

  sol[i2] = sol[i1];
  sol[i1] = x;

  return r;  
}


/**
 *  RESET
 *
 * Performs a reset (returns the new cost or -1 if unknown)
 */
int Reset(int n, AdData *p_ad)
{
  int i, j, k;
  int x, y, sz;
  int max = 0, imax = 0, nb_max = 0;
  int bound = p_ad->total_cost;
  int best_cost = ((unsigned) -1) >> 1;
  int best_i = 0, best_j = 0, best_dir = 0; /* these inits are only for the compiler ! */
  int cost;

  for(i = 0; i < size; i++)	/* find index of (one of) the most erroneous var */
    {
      if (err[i] > max)
        {
          max = err[i];
          imax = i;
          nb_max = 1;
        } 
      else if (err[i] == max && Random(++nb_max) == 0)
	imax = i;
    }

  for(k = 0; k < size; k++)
    {
      /* we need a random here to avoid to be trapped in the same "best" (see below best_cost) */
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

      x = sol[i];
      y = sol[j];

      /* shift left 1 cell */
      memmove(sol + i, sol + i + 1, sz);
      sol[j] = x;

      if ((cost = Cost(NULL)) < bound)
	return -1;      /* -1 because the err[] is not up-to-date */

      if (cost < best_cost)
	{
	  best_cost = cost; best_i = i; best_j = j; best_dir = 0;
	}

      /* shift right 1 cell (+ 1 to cancel previous shift left) */
      memmove(sol + i + 2, sol + i, sz - sizeof(int));
      sol[i + 1] = x;
      sol[i] = y;

      if ((cost = Cost(NULL)) < bound)
	return -1;		/* -1 because the err[] is not up-to-date */

      if (cost < best_cost)
	{
	  best_cost = cost; best_i = i; best_j = j; best_dir = 1;
	}

      /* cancel shift to re-init sol[] as it was at entry */
      memmove(sol + i, sol + i + 1, sz);      
      sol[j] = y;
    }

  sz = (best_j - best_i) * sizeof(int);
  if (best_dir == 0)		/* left */
    {
      x = sol[best_i];
      memmove(sol + best_i, sol + best_i + 1, sz);
      sol[best_j] = x;
    }
  else				/* right */
    {
      x = sol[best_j];
      memmove(sol + best_i + 1, sol + best_i, sz);
      sol[best_i] = x;
    }

  return -1;      /* -1 because the err[] is not up-to-date */
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

  p_ad->size = p_ad->param;
  p_ad->base_value = 1;

  /* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 50;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 1;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 5;

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 1000000000;

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


/**
 *  CHECK_SOLUTION
 *
 *  Checks if the solution is valid.
 */
int Check_Solution(AdData *p_ad)
{
  int size = p_ad->size;
  int *sol = p_ad->sol;
  int i, j, d;
  int r = 1;
  int nr;
  int dist;  

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
	  nr = nb_occ[d];
	  if (nr > 1)
	    {
	      dist = d - size;
	      printf("ERROR at row %d: distance %d appears %d times\n", i, dist, nr);
	      r = 0;
	    }
	}
    }

  return r;
}
