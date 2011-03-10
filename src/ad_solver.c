/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  ad_solver.c: general solver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <time.h>                  /* time() */
#include <sys/time.h>              /* gettimeofday() */
#include <unistd.h>                /* sleep() */

#define AD_SOLVER_FILE

#include "ad_solver.h"
#include "tools.h"
#include "ad_solver_MPI.h"

#if defined(CELL) && defined(__SPU__)

#ifdef CELL_COMM		// -- (un)define in the command line

#define CELL_COMM_SEND_WHEN    0   // 0: local min is reached, 1: reset occurs, K iters are reached
#define CELL_COMM_SEND_TO      0    // 0: neighbors only, 1: all
#define CELL_COMM_PROB_ACCEPT  80  // probability to accept the information
#define CELL_COMM_ACTION       0    // 0: restart, 1 reset, -1: nothing (test only)


#if CELL_COMM_SEND_TO == 0
#define CELL_COMM_SEND_CMD(v)  as_mbx_send_next(v)
#elif CELL_COMM_SEND_TO == 1
#define CELL_COMM_SEND_CMD(v)  	/*  if (v <= best_cost * 1.1) */as_mbx_send_all(v)
#endif


#if CELL_COMM_ACTION == 0
#define CELL_COMM_ACTION_CMD  goto restart
#elif  CELL_COMM_ACTION == 1
#define CELL_COMM_ACTION_CMD  Do_Reset(200)
#endif


#include "cell-extern.h"


#endif /* CELL_COMM */

#endif	/* CELL */

/*-----------*
 * Constants *
 *-----------*/

#define BIG ((unsigned int) -1 >> 1)

/*-------*
 * Types *
 *-------*/

typedef struct
{
  int i, j;
}Pair;

/*------------------*
 * Global variables *
 *------------------*/

static AdData ad ALIGN;		/* copy of the passed *p_ad (help optim ?) */

static int max_i ALIGN;		/* swap var 1: max projected cost (err_var[])*/
static int min_j ALIGN;		/* swap var 2: min conflict (swap[])*/
static int new_cost ALIGN;	/* cost after swapping max_i and min_j */
static int best_cost ALIGN;	/* best cost found until now */

static unsigned *mark ALIGN;	/* next nb_swap to use a var */
static int nb_var_marked ALIGN;	/* nb of marked variables */

#if defined(DEBUG) && (DEBUG&1)
static int *err_var;		/* projection of errors on variables */
static int *swap;		/* cost of each possible swap */
#endif

static int *list_i;		/* list of max to randomly chose one */
static int list_i_nb;		/* nb of elements of the list */

static int *list_j;		/* list of min to randomly chose one */
static int list_j_nb;		/* nb of elements of the list */

static Pair *list_ij;		/* list of max/min (exhaustive) */
static int list_ij_nb;		/* nb of elements of the list */

#ifdef LOG_FILE
static FILE *f_log;		/* log file */
#endif

//#define BASE_MARK    ((unsigned) ad.nb_iter)
#define BASE_MARK    ((unsigned) ad.nb_swap)
#define Mark(i, k)   mark[i] = BASE_MARK + (k)
#define UnMark(i)    mark[i] = 0
#define Marked(i)    (BASE_MARK + 1 <= mark[i])

#define USE_PROB_SELECT_LOC_MIN ((unsigned) ad.prob_select_loc_min <= 100)

#if defined PRINT_COSTS /* Print a sum up of (cost;iter) to picture the curve */
int vec_costs[500000] ;
unsigned long int card_vec_costs ;
#endif

/*------------*
 * Prototypes *
 *------------*/

#if defined(DEBUG) && (DEBUG&1)
static void Show_Debug_Info(void);
#endif

#if defined(DEBUG) && (DEBUG&1)
/*
 *  ERROR_ALL_MARKED
 */
static void
Error_All_Marked()
{
  int i;

  printf("\niter: %d - all variables are marked wrt base mark: %d\n",
	 ad.nb_iter, BASE_MARK);
  for (i = 0; i < ad.size; i++)
    printf("M(%d)=%d  ", i, mark[i]);
  printf("\n");
  exit(1);
}
#endif

#ifdef PRINT_COSTS
void print_costs()
{
  unsigned long int i ;
  
  char line[200] ;
  
  sprintf(line,"*** Costs for %d\n", my_num);
  writen(file_descriptor_print_cost, line, strlen(line)) ;

  for( i=0 ; i<=card_vec_costs ; i++ ) {
    sprintf(line,"%ld    %d\n", i, vec_costs[i]) ;
    writen(file_descriptor_print_cost, line, strlen(line)) ;
  }
  sprintf(line,"*** Fin Costs for %d\n", my_num);
  writen(file_descriptor_print_cost, line, strlen(line)) ;
}
#endif /* PRINT_COSTS */

/*
 * AD_UN_MARK
 */
void
Ad_Un_Mark(int i)
{
  UnMark(i);
}

/*
 *  SELECT_VAR_HIGH_COST
 *
 *  Computes err_swap and selects the maximum of err_var in max_i.
 *  Also computes the number of marked variables.
 */
static void
Select_Var_High_Cost(void)
{
  int i;
  int x, max;

  list_i_nb = 0;
  max = 0;
  nb_var_marked = 0;

  for(i = 0; i < ad.size; i++)
    {
      if (Marked(i))
	{
#if defined(DEBUG) && (DEBUG&1)
	  err_var[i] = Cost_On_Variable(i);
#endif
	  nb_var_marked++;
	  continue;
	}

      x = Cost_On_Variable(i);
#if defined(DEBUG) && (DEBUG&1)
      err_var[i] = x;
#endif

      if (x >= max)
	{
	  if (x > max)
	    {
	      max = x;
	      list_i_nb = 0;
	    }
	  list_i[list_i_nb++] = i;
	}
    }

  /* here list_i_nb == 0 iff all vars are marked or bad Cost_On_Variable() */

#if defined(DEBUG) && (DEBUG&1)
  if (list_i_nb == 0)
    Error_All_Marked();
#endif

  ad.nb_same_var += list_i_nb;
  x = Random(list_i_nb);
  max_i = list_i[x];
}

/*
 *  SELECT_VAR_MIN_CONFLICT
 *
 *  Computes swap and selects the minimum of swap in min_j.
 */
static void
Select_Var_Min_Conflict(void)
{
  int j;
  int x;

 a:
  list_j_nb = 0;
  new_cost = ad.total_cost;

  for(j = 0; j < ad.size; j++)
    {
#if defined(DEBUG) && (DEBUG&1)
      swap[j] = Cost_If_Swap(ad.total_cost, j, max_i);
#endif

#ifndef IGNORE_MARK_IF_BEST
      if (Marked(j))
	continue;
#endif

      x = Cost_If_Swap(ad.total_cost, j, max_i);

#ifdef IGNORE_MARK_IF_BEST
      if (Marked(j) && x >= best_cost)
	continue;
#endif

      if (USE_PROB_SELECT_LOC_MIN && j == max_i)
	continue;

      if (x <= new_cost)
	{
	  if (x < new_cost)
	    {
	      list_j_nb = 0;
	      new_cost = x;
	      if (ad.first_best)
		{
		  min_j = list_j[list_j_nb++] = j;
		  return;         
		}
	    }

	  list_j[list_j_nb++] = j;
	}
    }

  if (USE_PROB_SELECT_LOC_MIN)
    {
      if (new_cost >= ad.total_cost && 
	  (Random(100) < (unsigned) ad.prob_select_loc_min ||
	   (list_i_nb <= 1 && list_j_nb <= 1)))
	{
	  min_j = max_i;
	  return;
	}

      if (list_j_nb == 0)		/* here list_i_nb >= 1 */
	{
#if 0
	  min_j = -1;
	  return;
#else
	  ad.nb_iter++;
	  x = Random(list_i_nb);
	  max_i = list_i[x];
	  goto a;
#endif
	}
    }

  x = Random(list_j_nb);
  min_j = list_j[x];
}

/*
 *  SELECT_VARS_TO_SWAP
 *
 *  Computes max_i and min_j, the 2 variables to swap.
 *  All possible pairs are tested exhaustively.
 */
static void
Select_Vars_To_Swap(void)
{
  int i, j;
  int x;

  list_ij_nb = 0;
  new_cost = BIG;
  nb_var_marked = 0;

  i = -1;
  while((unsigned) (i = Next_I(i)) < (unsigned) ad.size) // false if i < 0
    {
      if (Marked(i))
	{
	  nb_var_marked++;
#ifndef IGNORE_MARK_IF_BEST
	  continue;
#endif
	}
      j = -1;
      while((unsigned) (j = Next_J(i, j)) < (unsigned) ad.size) // false if j < 0
	{
#ifndef IGNORE_MARK_IF_BEST
	  if (Marked(j))
	    continue;
#endif
	  x = Cost_If_Swap(ad.total_cost, i, j);

#ifdef IGNORE_MARK_IF_BEST
	  if (Marked(j) && x >= best_cost)
	    continue;
#endif

	  if (x <= new_cost)
	    {
	      if (x < new_cost)
		{
		  new_cost = x;
		  list_ij_nb = 0;
		  if (ad.first_best == 1 && x < ad.total_cost)
		    {
		      max_i = i;
		      min_j = j;
		      return; 
		    }
		}
	      list_ij[list_ij_nb].i = i;
	      list_ij[list_ij_nb].j = j;
	      list_ij_nb = (list_ij_nb + 1) % ad.size;
	    }
	}
    }

  ad.nb_same_var += list_ij_nb;

#if 0
  if (new_cost >= ad.total_cost)
    printf("   *** LOCAL MIN ***  iter: %d  next cost:%d >= total cost:%d #candidates: %d\n", ad.nb_iter, new_cost, ad.total_cost, list_ij_nb);
#endif

  if (new_cost >= ad.total_cost)
    {
      if (list_ij_nb == 0 || 
	  (USE_PROB_SELECT_LOC_MIN && Random(100) < (unsigned) ad.prob_select_loc_min))
	{
	  for(i = 0; Marked(i); i++)
	    {
#if defined(DEBUG) && (DEBUG&1)
	      if (i > ad.size)
		Error_All_Marked();
#endif
	    }
	  max_i = min_j = i;
	  goto end;
	}

      if (!USE_PROB_SELECT_LOC_MIN && (x = Random(list_ij_nb + ad.size)) < ad.size)
	{
	  max_i = min_j = x;
	  goto end;
	}
    }

  x = Random(list_ij_nb);
  max_i = list_ij[x].i;
  min_j = list_ij[x].j;

 end:
#if defined(DEBUG) && (DEBUG&1)
  swap[min_j] = new_cost;
#else
  ;				/* anything for the compiler */
#endif
}

/*
 *  AD_SWAP
 *
 *  Swaps 2 variables.
 */
void
Ad_Swap(int i, int j)
{
  int x;

  ad.nb_swap++;
  x = ad.sol[i];
  ad.sol[i] = ad.sol[j];
  ad.sol[j] = x;
}

static void
Do_Reset(int n)
{
#if defined(DEBUG) && (DEBUG&1)
  if (ad.debug)
    printf(" * * * * * * RESET n=%d\n", n);
#endif

  int cost = Reset(n, &ad);

#if UNMARK_AT_RESET == 2
  memset(mark, 0, ad.size * sizeof(unsigned));
#endif
  ad.nb_reset++;
  ad.total_cost = (cost < 0) ? Cost_Of_Solution(1) : cost;
}

/*
 *  EMIT_LOG
 *
 */
#ifdef LOG_FILE
#  define Emit_Log(...)					\
  do if (f_log)						\
    {							\
      fprintf(f_log, __VA_ARGS__);			\
      fputc('\n', f_log);				\
      fflush(f_log);					\
  } while(0)
#else
#  define Emit_Log(...)
#endif

/*
 *  SOLVE
 *
 *  General solve function.
 *  returns the final total_cost (0 on success)
 */
int
Ad_Solve(AdData *p_ad)
{
  int nb_in_plateau;
#if defined MPI
  Ad_Solve_MPIData mpi_data ;
  Ad_Solve_init_MPI_data( & mpi_data ) ;
#endif

  ad = *p_ad;	   /* does this help gcc optim (put some fields in regs) ? */

  ad_sol = ad.sol; /* copy of p_ad->sol and p_ad->reinit_after_if_swap (used by no_cost_swap) */
  ad_reinit_after_if_swap = p_ad->reinit_after_if_swap;


  if (ad_no_cost_var_fct)
    ad.exhaustive = 1;


  mark = (unsigned *) malloc(ad.size * sizeof(unsigned));
  if (ad.exhaustive <= 0)
    {
      list_i = (int *) malloc(ad.size * sizeof(int));
      list_j = (int *) malloc(ad.size * sizeof(int));
    }
  else
    list_ij = (Pair *) malloc(ad.size * sizeof(Pair)); // to run on Cell limit to ad.size instead of ad.size*ad.size

#if defined(DEBUG) && (DEBUG&1)
  err_var = (int *) malloc(ad.size * sizeof(int));
  swap = (int *) malloc(ad.size * sizeof(int));
#endif

  if (mark == NULL || (!ad.exhaustive && (list_i == NULL || list_j == NULL)) || (ad.exhaustive && list_ij == NULL)
#if defined(DEBUG) && (DEBUG&1)
      || err_var == NULL || swap == NULL
#endif
      )
    {
      fprintf(stderr, "%s:%d: malloc failed\n", __FILE__, __LINE__);
      exit(1);
    }

  memset(mark, 0, ad.size * sizeof(unsigned)); /* init with 0 */

#ifdef LOG_FILE
  f_log = NULL;
  if (ad.log_file)
    if ((f_log = fopen(ad.log_file, "w")) == NULL)
      perror(ad.log_file);
#endif

#if defined PRINT_COSTS
  card_vec_costs=-1 ;
#endif

  ad.nb_restart = -1;

  ad.nb_iter = 0;
  ad.nb_swap = 0;
  ad.nb_same_var = 0;
  ad.nb_reset = 0;
  ad.nb_local_min = 0;

  ad.nb_iter_tot = 0;
  ad.nb_swap_tot = 0;
  ad.nb_same_var_tot = 0;
  ad.nb_reset_tot = 0;
  ad.nb_local_min_tot = 0;

#if defined(DEBUG) && (DEBUG&2)
  if (ad.do_not_init)
    {
      printf("********* received data (do_not_init=1):\n");
      Ad_Display(ad.sol, &ad, NULL);
      printf("******************************-\n");
    }
#endif

  if (!ad.do_not_init)
    {
    restart:
      ad.nb_iter_tot += ad.nb_iter; 
      ad.nb_swap_tot += ad.nb_swap; 
      ad.nb_same_var_tot += ad.nb_same_var;
      ad.nb_reset_tot += ad.nb_reset;
      ad.nb_local_min_tot += ad.nb_local_min;

      Random_Permut(ad.sol, ad.size, ad.actual_value, ad.base_value);
      memset(mark, 0, ad.size * sizeof(unsigned)); /* init with 0 */
    }

  ad.nb_restart++;
  ad.nb_iter = 0;
  ad.nb_swap = 0;
  ad.nb_same_var = 0;
  ad.nb_reset = 0;
  ad.nb_local_min = 0;


  nb_in_plateau = 0;

  best_cost = ad.total_cost = Cost_Of_Solution(1);
  int best_of_best = BIG;


  while(ad.total_cost)
    {
      if (best_cost < best_of_best)
	{
	  best_of_best = best_cost;
#if 0 //******************************
	  printf("exec: %3d  iter: %10d  BEST %d (#locmin:%d  resets:%d)\n",  ad.nb_restart, ad.nb_iter, best_of_best, ad.nb_local_min, ad.nb_reset);
	  Display_Solution(&ad);

#endif
	}

      ad.nb_iter++;

#if defined PRINT_COSTS
      card_vec_costs++ ;
      vec_costs[card_vec_costs] = ad.total_cost ;
#endif

#if defined MPI
      if( Ad_Solve_manage_MPI_communications( & mpi_data, & ad ) == 10 )
	goto restart ;
#endif MPI

#ifdef CELL_COMM
      int comm_cost = (1 << 30);
      while(as_mbx_avail())
	{
	  int c= as_mbx_read();
	  if (c < comm_cost)
	    comm_cost = c;
	  usleep(1000);
	}
      if (1)
	{
	  //	  int comm_cost = as_mbx_read();
#ifdef CELL_COMM_ACTION_CMD
	  if (ad.total_cost > comm_cost && Random(100) < (unsigned) CELL_COMM_PROB_ACCEPT)
	    {
	      int n;
	      //n = (ad.size - (comm_cost * ad.size / ad.total_cost)) / 10;
	      n = 50;
	      if (n < 0 || n > ad.size)
		//		printf("CELL COMM: received a better cost (%d < %d): reset %d vars !\n", comm_cost, ad.total_cost, n);
		//Do_Reset(n);
		goto restart;

	      //CELL_COMM_ACTION_CMD;
	    }
#endif  /* CELL_COMM_ACTION_CMD */
	}
#endif	/* CELL_COMM */

#if defined(CELL_COMM) && CELL_COMM_SEND_WHEN > 1
      if (ad.nb_iter % CELL_COMM_SEND_WHEN == 0)
	{
	  //printf("CELL COMM: iter:%d - sending at every %d - cost:%d\n", ad.nb_iter, CELL_COMM_SEND_WHEN, ad.total_cost);
	  CELL_COMM_SEND_CMD(ad.total_cost);
	}
#endif


      if (ad.nb_iter >= ad.restart_limit)
	{
	  if (ad.nb_restart < ad.restart_max)
	    goto restart;
	  break;
	}

      if (!ad.exhaustive)
	{
	  Select_Var_High_Cost();
	  Select_Var_Min_Conflict();
	}
      else
	{
	  Select_Vars_To_Swap();
	}

      Emit_Log("----- iter no: %d, cost: %d, nb marked: %d ---",
	       ad.nb_iter, ad.total_cost, nb_var_marked);

#ifdef TRACE
      printf("----- iter no: %d, cost: %d, nb marked: %d --- swap: %d/%d  nb pairs: %d  new cost: %d\n", 
             ad.nb_iter, ad.total_cost, nb_var_marked,
             max_i, min_j, list_ij_nb, new_cost);
#endif
#ifdef TRACE
      Display_Solution(p_ad);
#endif

      if (ad.total_cost != new_cost)
	{
	  if (nb_in_plateau > 1)
	    {
	      Emit_Log("\tend of plateau, length: %d", nb_in_plateau);
	    }
	  nb_in_plateau = 0;
	}

      if (new_cost < best_cost)
	{
	  best_cost = new_cost;
	}


      if (!ad.exhaustive)
	{
	  Emit_Log("\tswap: %d/%d  nb max/min: %d/%d  new cost: %d",
		   max_i, min_j, list_i_nb, list_j_nb, new_cost);
	}
      else
	{
	  Emit_Log("\tswap: %d/%d  nb pairs: %d  new cost: %d",
		   max_i, min_j, list_ij_nb, new_cost);
	}


#if defined(DEBUG) && (DEBUG&1)
      if (ad.debug)
	Show_Debug_Info();
#endif

#if 0
      if (new_cost >= ad.total_cost && nb_in_plateau > 15)
	{
	  Emit_Log("\tTOO BIG PLATEAU - RESET");
	  Do_Reset(ad.nb_var_to_reset);
	}
#endif
      nb_in_plateau++;

      if (min_j == -1)
	continue;

      if (max_i == min_j)
	{
	  ad.nb_local_min++;
	  Mark(max_i, ad.freeze_loc_min);

#if defined(CELL_COMM) && CELL_COMM_SEND_WHEN == 0
	  CELL_COMM_SEND_CMD(ad.total_cost);
#endif

	  if (nb_var_marked + 1 >= ad.reset_limit)
	    {
	      Emit_Log("\tTOO MANY FROZEN VARS - RESET");

#if defined(CELL_COMM) && CELL_COMM_SEND_WHEN == 1
	      CELL_COMM_SEND_CMD(ad.total_cost);
#endif

	      Do_Reset(ad.nb_var_to_reset);
	    }
	}
      else
	{
	  Mark(max_i, ad.freeze_swap);
	  Mark(min_j, ad.freeze_swap);
	  Ad_Swap(max_i, min_j);
	  Executed_Swap(max_i, min_j);
	  ad.total_cost = new_cost;
	}
    } /* while(ad.total_cost) */

#ifdef LOG_FILE
  if (f_log)
    fclose(f_log);
#endif

  free(mark);
  free(list_i);
  if (!ad.exhaustive)
    free(list_j);
  else
    free(list_ij);

#if defined(DEBUG) && (DEBUG&1)
  free(err_var);
  free(swap);
#endif


  ad.nb_iter_tot += ad.nb_iter; 
  ad.nb_swap_tot += ad.nb_swap; 
  ad.nb_same_var_tot += ad.nb_same_var;
  ad.nb_reset_tot += ad.nb_reset;
  ad.nb_local_min_tot += ad.nb_local_min;

  *p_ad = ad;
  return ad.total_cost;
}



/*
 *  AD_DISPLAY
 *
 */
void
Ad_Display(int *t, AdData *p_ad, unsigned *mark)
{
  int i, k = 0, n;
  char buff[100];

  sprintf(buff, "%d", p_ad->base_value + p_ad->size - 1);
  n = strlen(buff);
  
  for(i = 0; i < p_ad->size; i++)
    {
      printf("%*d", n, t[i]);
      if (mark)
	{
	  if (Marked(i))
	    printf(" X ");
	  else
	    printf("   ");
	}
      else
	printf(" ");

      if (++k == p_ad->break_nl)
	{
	  putchar('\n');
	  k = 0;
	}
    }
  if (k)
    putchar('\n');
}




/*
 *  SHOW_DEBUG_INFO
 *
 *  Displays debug info.
 */
#if defined(DEBUG) && (DEBUG&1)
static void
Show_Debug_Info(void)
{
  char buff[100];

  printf("\n--- debug info --- iteration no: %d  swap no: %d\n", ad.nb_iter, ad.nb_swap);
  Ad_Display(ad.sol, &ad, mark);
  if (!ad_no_displ_sol_fct)
    {
      printf("user defined Display_Solution:\n");
      Display_Solution(&ad);
    }
  printf("total_cost: %d\n\n", ad.total_cost);
  if (!ad.exhaustive)
    {
      Ad_Display(err_var, &ad, mark);
      printf("chosen for max error: %d, error: %d\n\n",
	     max_i, err_var[max_i]);
      Ad_Display(swap, &ad, mark);
      printf("chosen for min conflict: %d, cost: %d\n",
	     min_j, swap[min_j]);
    }
  else
    {
      printf("chosen for swap: %d<->%d, cost: %d\n", 
	     max_i, min_j, swap[min_j]);
    }

  if (max_i == min_j)
    printf("\nfreezing var %d for %d swaps\n", max_i, ad.freeze_loc_min);

  if (ad.debug == 2)
    {
      printf("\nreturn: next step, c: continue (as with -d), s: stop debugging, q: quit: ");
      if (fgets(buff, sizeof(buff), stdin)) /* avoid gcc warning warn_unused_result */
	{}
      switch(*buff)
	{
	case 'c':
	  ad.debug = 1;
	  break;
	case 's':
	  ad.debug = 0;
	  break;
	case 'q':
	  exit(1);
	}
    }
}
#endif


