/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  qap.c: the Quadratic Assignment Problem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad_solver.h"

#ifdef MPI
#include <sys/time.h>
#endif

#define QAP_NO_MAIN
#include "qap-utils.c"


//#define SPEED 0
//#define SPEED 1 
#define SPEED 2 

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
static QAPInfo qap_info;
static int *sol;		/* copy of p_ad->sol */

static QAPMatrix mat_A, mat_B;

#if SPEED == 2
static QAPMatrix delta;
#endif

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

  if (mat_A == NULL)		/* matrices not yet read */
    {
      QAP_Load_Problem(p_ad->param_file, &qap_info);

      mat_A = qap_info.a;
      mat_B = qap_info.b;
      
#if SPEED == 2
      delta = QAP_Alloc_Matrix(size);
#endif
    }

  Ad_Solve(p_ad);
}





/*  The following functions are strongly inspired from of E. Taillard's
 *  Robust Taboo Search code.
 *  http://mistic.heig-vd.ch/taillard/codes.dir/tabou_qap2.c
 */

/*
 *  Compute the cost difference if elements i and j are permuted
 */

int
Compute_Delta(int i, int j)
{
  int pi = sol[i];
  int pj = sol[j];
  int k, pk;
  int d = 
    (mat_A[i][i] - mat_A[j][j]) * (mat_B[pj][pj] - mat_B[pi][pi]) +
    (mat_A[i][j] - mat_A[j][i]) * (mat_B[pj][pi] - mat_B[pi][pj]);

#if 0
  for(k = 0; k < size; k++)
    {
      if (k != i && k != j)
	{
	  pk = sol[k];
	  d +=
	    (mat_A[k][i] - mat_A[k][j]) * (mat_B[pk][pj] - mat_B[pk][pi]) +
	    (mat_A[i][k] - mat_A[j][k]) * (mat_B[pj][pk] - mat_B[pi][pk]);
	}
    }
#else    // here we know that i < j
  for(k = 0; k < i; k++)
    {
      pk = sol[k];
      d +=
	(mat_A[k][i] - mat_A[k][j]) * (mat_B[pk][pj] - mat_B[pk][pi]) +
	(mat_A[i][k] - mat_A[j][k]) * (mat_B[pj][pk] - mat_B[pi][pk]);
    }

  while(++k < j)
    {
      pk = sol[k];
      d +=
	(mat_A[k][i] - mat_A[k][j]) * (mat_B[pk][pj] - mat_B[pk][pi]) +
	(mat_A[i][k] - mat_A[j][k]) * (mat_B[pj][pk] - mat_B[pi][pk]);
    }
  while(++k < size)
    {
      pk = sol[k];
      d +=
	(mat_A[k][i] - mat_A[k][j]) * (mat_B[pk][pj] - mat_B[pk][pi]) +
	(mat_A[i][k] - mat_A[j][k]) * (mat_B[pj][pk] - mat_B[pi][pk]);
    }
#endif

  return d;
}

#if SPEED == 2
/*
 *  As above, compute the cost difference if elements i and j are permuted
 *  but the value of delta[i][j] is supposed to be known before
 *  the transposition of elements r and s.
 */
int
Compute_Delta_Part(int i, int j, int r, int s)
{
  int pi = sol[i];
  int pj = sol[j];
  int pr = sol[r];
  int ps = sol[s];

  return delta[i][j] + 
    (mat_A[r][i] - mat_A[r][j] + mat_A[s][j] - mat_A[s][i]) *
    (mat_B[ps][pi] - mat_B[ps][pj] + mat_B[pr][pj] - mat_B[pr][pi]) +
    (mat_A[i][r] - mat_A[j][r] + mat_A[j][s] - mat_A[i][s]) *
    (mat_B[pi][ps] - mat_B[pj][ps] + mat_B[pj][pr] - mat_B[pi][pr]);
}
#endif



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
  int i, j;
  int r = 0;

  for(i = 0; i < size; i++)
    for(j = 0; j < size; j++)
      r += mat_A[i][j] * mat_B[sol[i]][sol[j]];
  
#if SPEED == 2
  if (should_be_recorded)
    for(i = 0; i < size; i++)
      for(j = i + 1; j < size; j++)
	delta[i][j] = Compute_Delta(i, j);
#endif


  return r;
}




#if SPEED >= 1

/*
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */

int
Cost_If_Swap(int current_cost, int i, int j)
{
#if SPEED == 1
  return current_cost + Compute_Delta(i, j);
#else
  return current_cost + delta[i][j];
#endif
}

#endif


#if SPEED == 2


/*
 *  EXECUTED_SWAP
 *
 *  Records a swap.
 */

void
Executed_Swap(int i1, int i2)
{
  int i, j;

  for (i = 0; i < size; i++)
    for (j = i + 1; j < size; j++)
      if (i != i1 && i != i2 && j != i1 && j != i2)
	delta[i][j] = Compute_Delta_Part(i, j, i1, i2);
      else
	delta[i][j] = Compute_Delta(i, j);

}
#endif



int param_needed = -1;		/* overwrite var of main.c */



/*
 *  INIT_PARAMETERS
 *
 *  Initialization function.
 */

void
Init_Parameters(AdData *p_ad)
{
#ifdef MPI
  /*#ifdef YC_DEBUG*/
  struct timeval tv ;
  /*#endif*/
#endif

#ifdef MPI
  #/* #ifdef YC_DEBUG */
  gettimeofday(&tv, NULL);
  printf("%d begins %ld:%ld\n", my_num, (long int)tv.tv_sec,
	  (long int)tv.tv_usec) ;
  /*#endif */
#endif /* MPI */

  p_ad->size = QAP_Load_Problem(p_ad->param_file, NULL); /* get the problem size only */

				/* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 50;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = p_ad->size;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 4;

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 1000000000;

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
 
  return 1;
}
