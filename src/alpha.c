/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *			MPI Yves Caniou and Florian Richoux
 *
 *  alpha.c: alphacipher
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

#define NB_VAR 26
#define NB_CSTR 20
#define A 0
#define B 1
#define C 2
#define D 3
#define E 4
#define F 5
#define G 6
#define H 7
#define I 8
#define J 9
#define K 10
#define L 11
#define M 12
#define N 13
#define O 14
#define P 15
#define Q 16
#define R 17
#define S 18
#define T 19
#define U 20
#define V 21
#define W 22
#define X 23
#define Y 24
#define Z 25


/*-------*
 * Types *
 *-------*/

typedef struct
{
  int left[10];
  int right;
} InfCstr;


typedef struct
{
  int times;
  int *err;
}XRef;


/*------------------*
 * Global variables *
 *------------------*/

static int	size;			/* copy of p_ad->size */
static int	*sol;			/* copy of p_ad->sol */
static int	err[NB_CSTR];		/* errors on constraints */
static XRef	xref[NB_VAR][NB_CSTR];
static InfCstr	cstr[NB_CSTR] =
{ { { B,A,L,L,E,T      , -1 },  45 },
  { { C,E,L,L,O        , -1 },  43 },
  { { C,O,N,C,E,R,T    , -1 },  74 },
  { { F,L,U,T,E        , -1 },  30 },
  { { F,U,G,U,E        , -1 },  50 },
  { { G,L,E,E          , -1 },  66 },
  { { J,A,Z,Z          , -1 },  58 },
  { { L,Y,R,E          , -1 },  47 },
  { { O,B,O,E          , -1 },  53 },
  { { O,P,E,R,A        , -1 },  65 },
  { { P,O,L,K,A        , -1 },  59 },
  { { Q,U,A,R,T,E,T    , -1 },  50 },
  { { S,A,X,O,P,H,O,N,E, -1 }, 134 },
  { { S,C,A,L,E        , -1 },  51 },
  { { S,O,L,O          , -1 },  37 },
  { { S,O,N,G          , -1 },  61 },
  { { S,O,P,R,A,N,O    , -1 },  82 },
  { { T,H,E,M,E        , -1 },  72 },
  { { V,I,O,L,I,N      , -1 }, 100 },
  { { W,A,L,T,Z        , -1 },  34 } };


/*------------*
 * Prototypes *
 *------------*/

/*
 *  MODELING
 *
 *  sol[i] = value of the ith variable (letter) (i in 0..NB_VAR-1)
 *           value in 1..NB_VAR
 *     
 *  The constraints are: for each equation the sum must be equal to the 
 *  given value.
 *
 *  err[j] = -value to reach + effective sum
 *
 *                NB_CSTR-1
 *  The total cost = Sum | err[j] |
 *                   j=0
 *
 *  The projection on a variable i
 *
 *                 NB_CSTR-1
 *  err_var[i] = |  Sum err[j] * F(i,j) |
 *                  j=0
 *  F(i,j) is the number of occurrences of variable i in the constraint j
 */


/**
 *  SOLVE
 *
 *  Initializations needed for the resolution.
 */
void Solve(AdData *p_ad)
{
  int i, j;
  int *p;
  int *er;
  XRef *q;

  sol = p_ad->sol;
  size = p_ad->size;

  if (xref[0][0].times == 0)	/* all constant arrays are defined */
    {
      for(j = 0; j < NB_CSTR; j++)
	{
	  for(p = cstr[j].left; *p >= 0; p++)
	    {
	      i = *p;	/* get the index of the var */
	      er = err + j;
	  
	      for(q = xref[i]; q->times != 0 && q->err != er; q++)
		;

	      q->times++;
	      q->err = er;
	    }
	}

#if DEBUG
      if (p_ad->debug)
	for(i = 0; i < NB_VAR; i++)
	  {
	    printf("var %c appears in: ", 'A' + i);
	    for(q = xref[i]; q->times != 0; q++)
	      {
		printf("%ld", (long) (q->err - err));
		if (q->times > 1)
		  printf("(x%d)", q->times);
		putchar(' ');
	      }
	    putchar('\n');
	  }
#endif
    }

  Ad_Solve(p_ad);
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
  int j, er;
  int *p;
  int r = 0;

  for(j = 0; j < NB_CSTR; j++)
    {
      er = -cstr[j].right;

      for(p = cstr[j].left; *p >= 0; p++)
	er += sol[*p];

      if (should_be_recorded)
	err[j] = er;
      r += abs(er);
    }

#if 0
  if (should_be_recorded)
    {
      for(j = 0; j < size; j++)
	printf("%d ", sol[j]);
      printf("= %d\n", r);
    }
#endif

  return r;
}


/**
 *  COST_ON_VARIABLE
 *
 *  Evaluates the error on a variable.
 */
int Cost_On_Variable(int i)
{
  XRef *q;
  int t;
  int r = 0;

#if 1
  for(q = xref[i]; (t = q->times) != 0 ; q++)
    r += t * *(q->err);
#else
  for(q = xref[i]; (t = q->times) != 0 ; q++)
    {
      int x = *(q->err);
      r += t *  x * x;
    }
#endif

  r = abs(r);
  return r;
}


#define Adjust(r, diff, x)   r = r - abs(x) + abs(x + (diff))
/**
 *  COST_IF_SWAP
 *
 *  Evaluates the new total cost for a swap.
 */
int Cost_If_Swap(int current_cost, int i1, int i2)
{
  int diff1, diff2, r;
  XRef *q1, *q2;
  int t1, t2;
  int *er1, *er2;

  r = current_cost;

  diff1 = sol[i2] - sol[i1];
  diff2 = -diff1;

  q1 = xref[i1];
  q2 = xref[i2];

  while((t1 = q1->times) != 0 && (t2 = q2->times) != 0)
    {
      er1 = q1->err;
      er2 = q2->err;

      if (er1 < er2)
	{
	  Adjust(r, diff1 * t1, *er1);
	  q1++;
	}
      else if (er2 < er1)
	{
	  Adjust(r, diff2 * t2, *er2);
	  q2++;
	}
      else
	{
	  t1 -= t2;
	  if (t1 > 0)
	    Adjust(r, diff1 * t1, *er1);
	  else
	    Adjust(r, diff2 * -t1, *er2);	    
	  q1++;
	  q2++;
	}
    }

  while((t1 = q1->times) != 0)
    {
      er1 = q1->err;
      Adjust(r, diff1 * t1, *er1);
      q1++;
    }

  while((t2 = q2->times) != 0)
    {
      er2 = q2->err;
      Adjust(r, diff2 * t2, *er2);
      q2++;
    }

  return r;
}


/**
 *  EXECUTED_SWAP
 *
 *  Records a swap.
 */
void Executed_Swap(int i1, int i2)
{
  int diff1, diff2;
  XRef *q;
  int t;
  int *er;

  diff1 =  sol[i1] - sol[i2];	/* swap already executed */
  diff2 = -diff1;

  for(q = xref[i1]; (t = q->times) != 0; q++)
    {
      er = q->err;
      *er += t * diff1;
    }

  for(q = xref[i2]; (t = q->times) != 0; q++)
    {
      er = q->err;
      *er += t * diff2;
    }
}


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

  p_ad->size = NB_VAR;
  p_ad->base_value = 1;
  
  /* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 10000; /* not used */

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 2;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = (NB_VAR / 4) + 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 5;

  if (p_ad->restart_limit == -1)
    p_ad->restart_limit = 10000000;

  if (p_ad->restart_max == -1)
    p_ad->restart_max = 0;
}


/**
 *  CHECK_SOLUTION
 *
 *  Checks if the solution is valid.
 */
int Check_Solution(AdData *p_ad)
{
  int j, x;
  int *p;
  int r = 1;

  for(j = 0; j < NB_CSTR; j++)
    {
      x = 0;

      for(p = cstr[j].left; *p >= 0; p++)
	x += p_ad->sol[*p];

      if (x != cstr[j].right)
	{
	  printf("ERROR constraint %d, sum: %d should be %d\n",
		 j + 1, x, cstr[j].right);
	  r = 0;
	}
    }

  return r;
}
