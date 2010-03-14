/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  queens.c: the N queens problem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad_solver.h"

/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/

typedef struct
{
  int *err;
  int  to_add;
}UpdateErr;


/*------------------*
 * Global variables *
 *------------------*/

static int size;		/* copy of p_ad->size (nb of queens) */
static int *sol;		/* copy of p_ad->sol */

static int size1;		/* size1: size-1 */
static int nb_diag;		/* nb of diagonals in a same direction */

static int *err_d1;		/* errors on diagonals 1 (\) */
static int *err_d2;		/* errors on diagonals 2 (/) */

#define D1(i, j)      (i + size1 - j)
#define D2(i, j)      (i + j)
#define ErrD1(i, j)   (err_d1[D1(i, j)])
#define ErrD2(i, j)   (err_d2[D2(i, j)])


/*------------*
 * Prototypes *
 *------------*/

/*
 *  MODELING
 *
 *  ad.sol[i] = j: there is a queen on line i column j (i, j in 0..ad.size-1)
 *  
 *  The constraints are: at most 1 queen on each diagonal (\ and /).
 *  There are nb_diag (2*ad.size-1) diagonals 1 (\) and 2 (/) numbered from 0
 *  as follows (on this example ad.size=5):
 *
 *   j=01234                                  01234
 *   i=0\\\\\    diagonal 1 (d1)	  i=0/////5   diagonal 2 (d2)    
 *     1\\\\\0   from i, j	 	    1/////6   from i, j          
 *     2\\\\\1   D1(i,j) = i+(ad.size-1)-j  2/////7   D2(i,j) = i+j
 *     3\\\\\2				    3/////8                      
 *     4\\\\\3				    4/////                       
 *       87654                       	  j=01234                        
 *
 *  err_d1/2[d] = nb of queens on the dth diagonal 1/2
 *
 *  Let F be the function defined as F(x) = 0 if x <= 1 and x otherwise
 *
 *                 nb_diag-1
 *  The total cost = Sum F(err_d1[d]) + F(err_d2[d])
 *                   d=0
 *
 *  The projection on a variable at i (i.e. a queen at i,j):
 *  err_var[i] = F(err_d1[D1(i,j)]) + F(err_d2[D2(i,j)])
 */

#if 0
#define F(x) (((1 - (x)) >> (sizeof(int) * 8 - 1)) & (x))
#else
static int			/* faster on pentium */
F(int x)
{
  return (x <= 1) ? 0 : x;
}
#endif



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

  size1 = size - 1;

  nb_diag = 2 * size - 1;

  if (err_d1 == NULL)
    {
      err_d1 = (int *) malloc(nb_diag * sizeof(int));
      err_d2 = (int *) malloc(nb_diag * sizeof(int));
      if (err_d1 == NULL || err_d2 == NULL)
	{
	  fprintf(stderr, "%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }

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
  int d, i, j, er, r;

  memset(err_d1, 0, nb_diag * sizeof(int));
  memset(err_d2, 0, nb_diag * sizeof(int));

  for(i = 0; i < size; i++)
    {
      j = sol[i];
      ErrD1(i, j)++;
      ErrD2(i, j)++;
    }

  r = 0;
  for(d = 1; d < nb_diag - 1; d++)
    {
      er = err_d1[d];
      r += F(er);

      er = err_d2[d];
      r += F(er);
    }

  return r;
}




/*
 *  COST_ON_VARIABLE
 *
 *  Evaluates the error on a variable.
 */
int
Cost_On_Variable(int i)
{
  int j, r, x;

  j = sol[i];
  
  x = ErrD1(i, j);
  r = F(x);

  x = ErrD2(i, j);
  r += F(x);

  return r;
}




/*
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */

#define Update_Error_Table(derr, toadd)		\
  p = start;					\
  for(;;)					\
    {						\
      if (p == end)				\
	{					\
	  p->err = derr;			\
	  p->to_add = toadd;			\
	  end++;			       	\
 	  break;				\
	}					\
      if (p->err == derr)			\
	{					\
	  p->to_add += toadd;			\
	  break;				\
	}					\
      p++;					\
    }

int
Cost_If_Swap(int current_cost, int i1, int i2)
{
  int r, x;
  int j1, j2;
  UpdateErr update_tbl[8], *start, *end, *p;
  int *err;

  end = update_tbl;

  j1 = sol[i1];
  j2 = sol[i2];

				/* update info for diagonal 1 */
  start = update_tbl;
  Update_Error_Table(&ErrD1(i1, j1), -1);
  Update_Error_Table(&ErrD1(i2, j2), -1);
  Update_Error_Table(&ErrD1(i1, j2), +1);
  Update_Error_Table(&ErrD1(i2, j1), +1);


				/* update info diagonal 2 */
  start = end;
  Update_Error_Table(&ErrD2(i1, j1), -1);
  Update_Error_Table(&ErrD2(i2, j2), -1);
  Update_Error_Table(&ErrD2(i1, j2), +1);
  Update_Error_Table(&ErrD2(i2, j1), +1);



#if 0
  printf("diagonals to update for swap %d/%d and %d/%d :\n",
	 i1, j1, i2, j2);
  for(p = update_tbl; p != end; p++)
    {
      err = p->err;
      if (p < start)
	{
	  x = err - err_d1;
	  r = 1;
	}
      else
	{
	  x = err - err_d2;
	  r = 2;
	}

      printf("on d%d[%d] add: %d\n", r, x, p->to_add);
    }
#endif  
  r = current_cost;

  for(p = update_tbl; p != end; p++)
    {
      err = p->err;
      x = *err;
      r -= F(x);

      x += p->to_add;
      r += F(x);
    }

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
  int j1, j2;
  
  j1 = sol[i2];		/* swap already executed */
  j2 = sol[i1];

  ErrD1(i1, j1)--;
  ErrD2(i1, j1)--;
  ErrD1(i2, j2)--;
  ErrD2(i2, j2)--;

  ErrD1(i1, j2)++;
  ErrD2(i1, j2)++;
  ErrD1(i2, j1)++;
  ErrD2(i2, j1)++;
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

  p_ad->first_best = 1;

  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 6;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 3;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = p_ad->size / 5;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 10;

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
  int i1, j1, i2, j2;

  for(i1 = 0; i1 < p_ad->size; i1++)
    {
      j1 = p_ad->sol[i1];
      for(i2 = i1 + 1; i2 < p_ad->size; i2++)
	{
	  j2 = p_ad->sol[i2];
	  if (abs(j2 - j1) == i2 - i1)
	    {
	      printf("ERROR conflict %d/%d and %d/%d\n", i1, j1, i2, j2);
	      return 0;
	    }
	}  
    }
  return 1;
}
