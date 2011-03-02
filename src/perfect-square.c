/*
 *  Adaptive search
 *
 *  Copyright (C) 2009 Daniel Diaz, Philippe Codognet & Salvador Abreu
 *
 *  perfect_square.c: the perfect square problem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ad_solver.h"

#define ACTUAL_VALUES

#ifdef MPI
#include <sys/time.h>
#endif

/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/


/* data taken from http://www.cs.st-andrews.ac.uk/~ianm/CSPLib/prob/prob009/results
 * keeping original pb number (-1 if not from the above site)
 */

typedef struct
{
  int orig_pb_number;		/* not used */
  int nb_squares;
  int master_square_size;
  int square_size[25];
} PbData;


/*------------------*
 * Global variables *
 *------------------*/

static int size;		/* copy of p_ad->size */
static int *sol;		/* copy of p_ad->sol */



/* sol for pb 1 (112) (no then sizes)
20 17 14  4 11  7  9  6  2 12 15 13  5  0  3 10  8 19  1 18 16 
50 35 27  8 19 15 17 11  6 24 29 25  9  2  7 18 16 42  4 37 33 

*/


static PbData pb[] = 
  {
    {  -1, 21,  16, {1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6} }, // pb #0
    {   1, 21, 112, {2,4,6,7,8,9,11,15,16,17,18,19,24,25,27,29,33,35,37,42,50} },  // pb #1
    {  19, 23, 228, {2,7,9,10,15,16,17,18,22,23,25,28,36,39,42,56,57,68,69,72,73,87,99} }, // pb #2
    {  41, 24, 326, {1,6,10,11,14,15,18,24,29,32,43,44,53,56,63,65,71,80,83,101,104,106,119,142} }, // pb #3
    {  47, 24, 479, {5,6,17,23,24,26,28,29,35,43,44,52,60,68,77,86,130,140,150,155,160,164,174,175} }, // pb #4
    { 154, 25, 524, {9,12,20,21,33,35,37,39,54,55,61,62,87,90,98,101,125,132,135,141,145,159,163,164,220} } // pb #5
  };

static int nb_pb = sizeof(pb) / sizeof(pb[0]);
static int pb_no;
static int master_square_size;
static int nb_squares;

static int col_y[600];		/* size should be >= at greatest master_square_size */
static int col_x[600];
static int y_max;


#ifndef ACTUAL_VALUES
#   define SIZE(i) pb[pb_no].square_size[sol[i]]
#else
#   define SIZE(i) sol[i]
#endif



/*------------*
 * Prototypes *
 *------------*/


/*
 *  MODELING
 *
 */


#if 0
#define DETAIL
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

  pb_no = p_ad->param;
#ifdef ACTUAL_VALUES
  p_ad->actual_value = pb[pb_no].square_size; /* reinit to avoid to pass it to threads */
#endif

  master_square_size = pb[pb_no].master_square_size;
  nb_squares = pb[pb_no].nb_squares;

  Ad_Solve(p_ad);
}



static __inline__
int Place_Squares(int *sol, int size, int master_square_size, char **ascii_repres)
{
  int i, sz, c, x_pos, y_pos;
  
  memset((void *) col_y, 0, master_square_size * sizeof(int));
  memset((void *) col_x, 0, master_square_size * sizeof(int));

  y_max = 0;

  for(i = 0; i < size; i++)
    {
      sz = SIZE(i);

      y_pos = master_square_size - sz + 1; /* max possible on the y-axis, look for the min */
      x_pos = -1;

      for(c = 0; c <= master_square_size - sz; c++) /* look for the smallest y */
        {
          if (col_y[c] < y_pos)
            {
	      x_pos = c;
	      y_pos = col_y[c];

	      if (y_pos == 0)	/* optimization: exit now if 0 is found */
		break;
	    }
	}

      if (x_pos < 0)		/* the current square cannot be placed on the y-axis or the x-axis */
	return i;

      for(c = 1; c < sz; c++)	/* check if width is ok */
	if (col_y[x_pos + c] != y_pos)
	  return i;

#ifdef DETAIL
      printf("square sz:%2d  placed at x_pos:%d  y_pos:%d\n", sz, x_pos, y_pos);
#endif

      if (ascii_repres)
	{
	  int x, y;
	  for(x = x_pos; x < x_pos + sz; x++)
	    for(y = y_pos; y < y_pos + sz; y++)
	      ascii_repres[x][y] = 'A' - 1 + sol[i];
	}


      int stop_y;
      for(stop_y = y_pos + sz; y_pos < stop_y; y_pos++)
	if (x_pos >= col_x[y_pos]) /* placed in a hole */
	  col_x[y_pos] = x_pos + sz;

      if (y_pos > y_max)
	y_max = y_pos;

      while(sz--)
	col_y[x_pos++] = y_pos;
      
#ifdef DETAIL
      printf("col_y: ");
      for(c = 0; c < master_square_size; c++)
	printf("%2d ", col_y[c]);
      printf("\ny_max:%d\n", y_max);
#if 0
      printf("col_x: ");
      for(c = 0; c < master_square_size; c++)
	printf("%2d ", col_x[c]);
      printf("\n");
#endif
#endif      
    }

  return i;
}


/*
 *  COST_OF_SOLUTION
 *
 *  Returns the total cost of the current solution.
 */

int first_i;

int
Cost_Of_Solution(int should_be_recorded)
{
  int i, c;

#if 0
#define DETAIL
#endif
    
#ifdef DETAIL
  printf("\n--------------------------------\n");
  printf("sq no: ");
  for(i = 0; i < size; i++)
    printf("%2d ", sol[i]);
  printf("\n");
  printf("sq sz: ");
  for(i = 0; i < size; i++)
    printf("%2d ", SIZE(i));
  printf("\n");

#endif
  
  i = Place_Squares(sol, size, master_square_size, NULL);

  if (should_be_recorded)
    first_i = i;
  
  int nb_missing_sq = size - i;
  int nb_empty_rect = 0;
  int max_height = 0;
  int max_width = 0;
  int ry = 0;
  int rx = 0;
  int x, y;
  int one_size = 0;


  while(i < size)			/* find the largest non-placed square */
    {
      c = SIZE(i++);
      if (c > one_size) 
	one_size = c;
    }


  /* compute ry : sum of all y (unfilled y - consecutive same y are counted once)  */

  for(c = 0; c < master_square_size; c++)
    {
      if (col_y[c] == master_square_size)
	continue;

      for(x = c, y = col_y[c]; x < master_square_size && col_y[x] == y; x++)
	;

#ifdef DETAIL
      printf("vert rectangle at %3d/%d  width=%2d  height wrt y_max=%d   top=%d\n", 
	     c, y, x - c, y_max - y, master_square_size - y);
#endif
      
      nb_empty_rect++;

      y = master_square_size - y;
      if (y > max_height)
	max_height = y;

      ry += y;
      c = x - 1;
    }

  /* compute rx : sum of all x (unfilled x - consecutive same x are counted once, stop at y_max)  */

  for(c = 0; c < y_max; c++)
    {
      if (col_x[c] == master_square_size)
	continue;

      for(y = c, x = col_x[c]; y < master_square_size && col_x[y] == x; y++)
	;

#ifdef DETAIL
      int h = y - c;
      printf("horiz rectangle at %3d/%d  heigth=%2d  width=%d\n", x, c, h, master_square_size - h);
#endif

      x = master_square_size - x;
      if (x > max_width)
	max_width = x;

      rx += x;
      c = y - 1;
    }


  /* cost formulas (previous bench timmings were with use first formula and -l 2 -p 10)
   *
   * int rt = nb_missing_sq * (rx * one_size + ry * max_height * (nb_missing_sq + nb_empty_rect));
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx * one_size + ry * max_height);
   *
   * int rt = (nb_missing_sq * nb_missing_sq) * (rx * one_size + ry * (max_height + nb_empty_rect));
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx * one_size + ry * (nb_empty_rect + max_height));
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (ry * max_height) + nb_missing_sq * nb_empty_rect * rx + one_size;
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx * one_size + ry * (nb_empty_rect + max_height));
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx  + ry * (nb_empty_rect + max_height)) + one_size;
   *
   * int rt = ((nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx  + ry * max_height)) + one_size;
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx * nb_empty_rect + ry * max_height + one_size);
   *
   * int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx * nb_empty_rect + ry * (max_height + one_size));
   *
   */

  int rt = (nb_missing_sq * nb_missing_sq + nb_empty_rect) * (rx * nb_empty_rect + ry * (max_height + one_size));



#ifdef DETAIL
  printf("ry:%3d rx:%3d nb_missing_sq:%2d  nb_empty_rect:%2d y_max:%3d max_height:%3d  cost:%10d\n",
	 ry, rx, nb_missing_sq, nb_empty_rect, y_max, max_height, rt);
#endif


#if 0//def DETAIL
  if (rt < 0)
    {
      printf("ERROR cost %d < 0\n", rt);
      exit(1);
    }
#endif

  return rt;
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
  int pb_no = p_ad->param;
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

  if (pb_no < 0)
    pb_no = 0;
  if (pb_no >= nb_pb)
    pb_no = nb_pb - 1;


  p_ad->size = pb[pb_no].nb_squares;
#ifdef ACTUAL_VALUES
  p_ad->actual_value = pb[pb_no].square_size; /* init for -I (start with same config) */
#endif

  printf("problem no: %d   nb squares: %d   master square size: %d\n", 
	 pb_no, p_ad->size, pb[pb_no].master_square_size);

  /* defaults */
  if (p_ad->prob_select_loc_min == -1)
    p_ad->prob_select_loc_min = 100;

  if (p_ad->freeze_loc_min == -1)
    p_ad->freeze_loc_min = 2;

  if (p_ad->freeze_swap == -1)
    p_ad->freeze_swap = 0;

  if (p_ad->reset_limit == -1)
    p_ad->reset_limit = 1;

  if (p_ad->reset_percent == -1)
    p_ad->reset_percent = 5;

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
  int pb_no = p_ad->param;
  int master_square_size = pb[pb_no].master_square_size;
  int i = Place_Squares(p_ad->sol, p_ad->size, master_square_size, NULL);
 
  if (i >= size)
    return 1;

  while(i < size)
    {
      printf("ERROR: square of size:%d cannot be placed\n", sol[i]);
      i++;
    }

  return 0;
}




/*
 *  DISPLAY_SOLUTION
 *
 *  Displays a solution.
 */
void
Display_Solution(AdData *p_ad)
{
  int i;

  printf("square sizes:");
  for(i = 0; i < p_ad->size; i++)
    printf(" %3d", SIZE(i));
  printf("\n");

 #if 0
  static char **ascii_repres = NULL;
  int pb_no = p_ad->param;
  int master_square_size = pb[pb_no].master_square_size;
  int x, y;

  if (ascii_repres == NULL)
    {
      ascii_repres = (char **) malloc(master_square_size * sizeof(char *));
      if (ascii_repres == NULL)
	{
	  printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}

      for(x = 0; x < master_square_size; x++)
	{
	  ascii_repres[x] = (char *) malloc(master_square_size);
	  if (ascii_repres[x] == NULL)
	    {
	      printf("%s:%d malloc failed\n", __FILE__, __LINE__);
	      exit(1);
	    }
	}
    }

  for(x = 0; x < master_square_size; x++)
    memset(ascii_repres[x], ' ', master_square_size);
  

  i = Place_Squares(p_ad->sol, p_ad->size, master_square_size, ascii_repres);
  
  for(y = y_max - 1; y >= 0; y--)
    {
      printf("%3d|", y);
      for(x = 0; x < master_square_size; x++)
	printf("%c", ascii_repres[x][y]);
      printf("|\n");
    }

  printf("   +");
  for(x = 0; x < master_square_size; x++)
    printf("%d", x % 10);
  printf("\n     ");
  for(x = 10; x < master_square_size; x += 10)
    printf("%10d", x / 10);
  printf("\n");
#endif
}
