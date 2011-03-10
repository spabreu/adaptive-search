/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  magic_square.c: magic squares
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

/* for a k in 0..size-1 gives line, col, d1 (1/0), d2 (0/1) */

/* YC->DD: normal that we have a define of CELL right before an ifdef? */
#define CELL

#ifdef CELL

typedef struct
{
  short w1;
  short w2;
} XRef;


#define XSet(xr, line, col, diag1, diag2)   xr.w1 = (diag1 << 15) | line; xr.w2 = (diag2 << 15) | col
#define XGetL(xr)     (xr.w1 & 0x7f)
#define XGetC(xr)     (xr.w2 & 0x7f)
#define XIsOnD1(xr)   (xr.w1 < 0)
#define XIsOnD2(xr)   (xr.w2 < 0)

#elif 0 // BEST on x86_64 /*YC: remove the compilation of this part for N>=128 */

typedef unsigned int XRef;

#define XSet(xr, line, col, diag1, diag2)   xr = (diag1 << 31) | (col << 16) | (diag2 << 15) | line
#define XGetL(xr)     (xr & 0x7f)
#define XGetC(xr)     ((xr >> 16) & 0x7f)
#define XIsOnD1(xr)   ((int) xr < 0)
#define XIsOnD2(xr)   ((xr & 0x00008000) != 0)

#elif 0  // others...

typedef unsigned int XRef;

#define XSet(xr, line, col, diag1, diag2)   xr = (diag1 << 31) | (diag2 << 30) | (col << 15) | line
#define XGetL(xr)     (xr & 0x7f)
#define XGetC(xr)     ((xr >> 15) & 0x7f)
#define XIsOnD1(xr)   ((int) xr < 0)
#define XIsOnD2(xr)   ((xr & 0x40000000) != 0)

#elif 1 /* YC: activation du code pour N>=128 */

typedef struct
{
  unsigned int d1:1;
  unsigned int d2:1;
  unsigned int l:15;
  unsigned int c:15;
} XRef;


#define XSet(xr, line, col, diag1, diag2)   xr.l = line; xr.c = col; xr.d1 = diag1; xr.d2 = diag2
#define XGetL(xr)     xr.l
#define XGetC(xr)     xr.c
#define XIsOnD1(xr)   (xr.d1 != 0)
#define XIsOnD2(xr)   (xr.d2 != 0)

#endif /* FYC: activation du code pour N>=128 */


/*------------------*
 * Global variables *
 *------------------*/

static int size;		/* copy of p_ad->size (square_length*square_length) */
static int *sol;		/* copy of p_ad->sol */

static int square_length;	/* side of the square */
static int square_length_m1;    /* square_length - 1 */
static int square_length_p1;    /* square_length + 1 */
static int avg;			/* sum to reach for each l/c/d */

static int *err_l, *err_l_abs;  /* errors on lines (relative + absolute) */
static int *err_c, *err_c_abs;	/* errors on columns */
static int err_d1, err_d1_abs; 	/* error on d1 (\) */
static int err_d2, err_d2_abs;	/* error on d2 (/) */
static XRef *xref;


/*------------*
 * Prototypes *
 *------------*/


/*
 *  MODELING
 *
 *  sol[] = vector of values (by line),
 *             sol[0..square_length-1] contain the first line, 
 *             sol[square_length-2*square_length-1] contain the 2nd line, ...
 *             values are in 1..square_length*square_length
 *
 *  The constraints are: for each line, column, diagonal 1 (\) and 2 (/)
 *  the sum must be equal to avg = square_length * (square_length*square_length + 1) / 2;
 *
 *  err_l[i] = -avg + sum of line i
 *  err_c[j] = -avg + sum of column j
 *  err_d1   = -avg + sum of diagonal 1
 *  err_d2   = -avg + sum of diagonal 2
 *
 *                   square_length-1  square_length-1
 *  The total cost = Sum |err_l[i]| + Sum |err_c[i]| + |err_d1| + |err_d2|
 *                   i=0              j=0
 *
 *  The projection on a variable at i, j:
 *  // err_var[i][j] = | err_l[i] + err_c[j] + F1(i,j) + F2(i,j) |  SLOW version
 *  err_var[i][j] = | err_l[i] + err_c[j] + F1(i,j) + F2(i,j) |
 *  with F1(i,j) = err_d1 if i,j is on diagonal 1 (i.e. i=j) else = 0
 *  and  F2(i,j) = err_d2 if i,j is on diagonal 2 (i.e. j=square_length-1-i) else = 0
 */


/*
 *  SOLVE
 *
 *  Initializations needed for the resolution.
 */

void
Solve(AdData *p_ad)
{
  int i, j, k;
  XRef xr;

  sol = p_ad->sol;
  size = p_ad->size;

  square_length = p_ad->param;
  square_length_m1 = square_length - 1;
  square_length_p1 = square_length + 1;

  avg = p_ad->data32[0];

  if (err_l == NULL)
    {
      err_l = (int *) malloc(square_length * sizeof(int));
      err_c = (int *) malloc(square_length * sizeof(int));
      err_l_abs = (int *) malloc(square_length * sizeof(int));
      err_c_abs = (int *) malloc(square_length * sizeof(int));
      xref = (XRef *) malloc(p_ad->size * sizeof(XRef));

      if (err_l == NULL || err_c == NULL || err_l_abs == NULL || err_c_abs == NULL || xref == NULL)
	{
          fprintf(stderr, "%s:%d malloc failed\n", __FILE__, __LINE__);
          exit(1);

	}
    }

  for(k = 0; k < p_ad->size; k++)
    {
      i = k / square_length;
      j = k % square_length;

      XSet(xr, i, j, (i == j), (i + j == square_length_m1));

      xref[k] = xr;
    }

  Ad_Solve(p_ad);
  /* YC->DD: Normal qu'il n'y ait pas les free() suivants ?
  free( err_l ) ;
  free( err_c ) ;
  free( err_l_abs ) ;
  free( err_c_abs ) ;
  free( xref ) ; */
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
  int k, r;
  int neg_avg = -avg;

  err_d1 = err_d2 = neg_avg;

  memset(err_l, 0, sizeof(int) * square_length);
  memset(err_c, 0, sizeof(int) * square_length);

  k = 0;
  do
    {
      XRef xr = xref[k];

      err_l[XGetL(xr)] += sol[k];
      err_c[XGetC(xr)] += sol[k];
    }
  while(++k < size);

  int k1 = 0, k2 = 0;
  do
    {
      k2 += square_length_m1;
      err_d1 += sol[k1];
      err_d2 += sol[k2];

      k1 += square_length_p1;
    }
  while(k1 < size);

  err_d1_abs = abs(err_d1);
  err_d2_abs = abs(err_d2);

  r = err_d1_abs + err_d2_abs;
  k = 0;
  do
    {
      err_l[k] -= avg; err_l_abs[k] = abs(err_l[k]); r += err_l_abs[k];
      err_c[k] -= avg; err_c_abs[k] = abs(err_c[k]); r += err_c_abs[k];
    }
  while(++k < square_length);

  return r;
}



/*
 *  COST_ON_VARIABLE
 *
 *  Evaluates the error on a variable.
 */

int
Cost_On_Variable(int k)
{
  XRef xr = xref[k];
  int r;

#ifndef SLOW

  r = err_l_abs[XGetL(xr)] + err_c_abs[XGetC(xr)] + 
    (XIsOnD1(xr) ? err_d1_abs : 0) + 
    (XIsOnD2(xr) ? err_d2_abs : 0);
  
#else  // less efficient use it with -f 5 -p 10 -l (ad.size/4)+1

  r = err_l[XGetL(xr)] + err_c[XGetC(xr)] + 
    (XIsOnD1(xr) ? err_d1 : 0) + 
    (XIsOnD2(xr) ? err_d2 : 0);

  r = abs(r);

#endif


  return r;
}



/*
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */

#define AdjustL(r, diff, k)   r = r - err_l_abs[k] + abs(err_l[k] + diff)
#define AdjustC(r, diff, k)   r = r - err_c_abs[k] + abs(err_c[k] + diff)
#define AdjustD1(r, diff)     r = r - err_d1_abs   + abs(err_d1   + diff)
#define AdjustD2(r, diff)     r = r - err_d2_abs   + abs(err_d2   + diff)

int
Cost_If_Swap(int current_cost, int k1, int k2)
{
  XRef xr1 = xref[k1];
  XRef xr2 = xref[k2];
  int l1 = XGetL(xr1);
  int c1 = XGetC(xr1);
  int l2 = XGetL(xr2);
  int c2 = XGetC(xr2);
  int diff1, diff2, r;
  
  r = current_cost;

  diff1 = sol[k2] - sol[k1];
  diff2 = -diff1;

  if (l1 != l2)			/* not on the same line */
    {
      AdjustL(r, diff1, l1);
      AdjustL(r, diff2, l2);
    }

  if (c1 != c2)			/* not on the same column */
    {
      AdjustC(r, diff1, c1);
      AdjustC(r, diff2, c2);
    }


  if (XIsOnD1(xr1))		/* only one of both is on diagonal 1 */
    {
      if (!XIsOnD1(xr2))
	AdjustD1(r, diff1);
    }
  else if (XIsOnD1(xr2))
    {
      AdjustD1(r, diff2);
    }

  if (XIsOnD2(xr1))		/* only one of both is on diagonal 2 */
    {
      if (!XIsOnD2(xr2))
	AdjustD2(r, diff1);
    }
  else if (XIsOnD2(xr2))
    {
      AdjustD2(r, diff2);
    }

  return r;
}




/*
 *  EXECUTED_SWAP
 *
 *  Records a swap.
 */

void
Executed_Swap(int k1, int k2)
{
  XRef xr1 = xref[k1];
  XRef xr2 = xref[k2];
  int l1 = XGetL(xr1);
  int c1 = XGetC(xr1);
  int l2 = XGetL(xr2);
  int c2 = XGetC(xr2);
  int diff1, diff2;
  
  diff1 = sol[k1] - sol[k2]; /* swap already executed */
  diff2 = -diff1;

  err_l[l1] += diff1; err_l_abs[l1] = abs(err_l[l1]);
  err_l[l2] += diff2; err_l_abs[l2] = abs(err_l[l2]);
  
  err_c[c1] += diff1; err_c_abs[c1] = abs(err_c[c1]);
  err_c[c2] += diff2; err_c_abs[c2] = abs(err_c[c2]);
  
  if (XIsOnD1(xr1))
    {
      err_d1 += diff1;
      err_d1_abs = abs(err_d1);
    }

  if (XIsOnD1(xr2))
    {
      err_d1 += diff2;
      err_d1_abs = abs(err_d1);
    }

  if (XIsOnD2(xr1))
    {
      err_d2 += diff1;
      err_d2_abs = abs(err_d2);
    }


  if (XIsOnD2(xr2))
    {
      err_d2 += diff2;
      err_d2_abs = abs(err_d2);
    }



#if 0
  printf("----- after swapping %d and %d", k1, k2);
  int i;
  for(i = 0; i < size; i++)
    {
      if (i % square_length == 0)
	printf("\n");
      printf(" %d", sol[i]);
      
    }
  printf("\n");
  printf("err_lin:");
  for(i = 0; i < square_length; i++)
    printf(" %d", err_l[i]);
  printf("\n");

  printf("err_col:");
  for(i = 0; i < square_length; i++)
    printf(" %d", err_c[i]);
  printf("\n");

  printf("err_d1: %d   err_d2:%d\n", err_d1, err_d2);
  printf("----------------------------------------------\n");
#endif
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
  int square_length = p_ad->param;

  p_ad->size = square_length * square_length;

  int avg = square_length * (p_ad->size + 1) / 2;
  /* TODO: should be PRINT0 */
  printf("sum of each line/col/diag = %d\n", avg);

  p_ad->data32[0] = avg;


  p_ad->base_value = 1;
  p_ad->break_nl = square_length;
				/* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 6;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 1;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = square_length * 1.2;

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
  int square_length = p_ad->param;
  int *sol = p_ad->sol;
  int avg = square_length * (p_ad->size + 1) / 2;
  int i, j;
  int sum_d1 = 0, sum_d2 = 0;
  

  for(i = 0; i < square_length; i++)
    {
      sum_d1 += sol[i * (square_length + 1)];
      sum_d2 += sol[(i + 1) * (square_length - 1)];
      int sum_l = 0, sum_c = 0;

      for(j = 0; j < square_length; j++)
        {
          sum_l += sol[i * square_length + j];
          sum_c += sol[j * square_length + i];
        }

      if (sum_l != avg)
	{
	  printf("ERROR line %d, sum: %d should be %d\n", i, sum_l, avg);
	  return 0;
	}

      if (sum_c != avg)
	{
	  printf("ERROR column %d, sum: %d should be %d\n", i, sum_c, avg);
	  return 0;
	}
    }


  if (sum_d1 != avg)
    {
      printf("ERROR column 1 (\\), sum: %d should be %d\n", sum_d1, avg);
      return 0;
    }

  if (sum_d2 != avg)
    {
      printf("ERROR column 2 (/), sum: %d should be %d\n", sum_d2, avg);
      return 0;
    }

  return 1;
}
