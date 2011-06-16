/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  ad_solver.h: general solver
 */

#ifndef AS_AD_SOLVER_H
#define AS_AD_SOLVER_H

#include "tools.h"

#ifdef CELL
#include <malloc.h>
#define malloc(sz) memalign(16, (((sz) + 127) >> 7) << 7)
#define ALIGN  __attribute__ ((aligned (16)))
#else
#define ALIGN
#endif

/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/

typedef struct
{
				/* --- input: basic data --- */

  int size;			/* nb of variables */
  int do_not_init;		/* use the initial solution (else random permut) */
  int *actual_value;		/* if random permut: actual values (see tools.c) */
  int base_value;		/* if random permut: base value (see tools.c) */
  int debug;			/* debug level (0 1 2) */
  int break_nl;			/* to display a matrix (nb of columns or 0) */
  char *log_file;		/* name of the log file or NULL */

				/* --- input: tuning parameters --- */

  int exhaustive;		/* perform an exhausitve search */
  int first_best;		/* stop as soon as a better swap is found */
  int prob_select_loc_min;	/* % to select local min instead of staying on a plateau (or >100 to not use)*/
  int freeze_loc_min;		/* nb swaps to freeze a (local min) var */
  int freeze_swap;		/* nb swaps to freeze 2 swapped vars */
  int reset_limit;		/* nb of frozen vars before reset */
  int nb_var_to_reset;		/* nb variables to reset */
  int restart_limit;		/* nb of iterations before restart */
  int restart_max;		/* max nb of times to restart (to retry) */
  int reinit_after_if_swap;	/* true if Cost_Of_Solution must be called twice */

				/* --- input / output: solution --- */

  int *sol;			/* the array of variables */
  int size_in_bytes;		/* size in bytes of sol */

				/* --- output: info / counters --- */

  int total_cost;		/* total cost of the current solution */
  int nb_restart;		/* nb of restarts */

  int nb_iter;			/* nb of iterations (can also be used as current no for marks) */
  int nb_swap;			/* nb of swaps (used as current no for marks) */
  int nb_same_var;		/* nb of vars with highest cost */
  int nb_reset;			/* nb of resets */
  int nb_local_min;		/* nb of local mins */

				/* same counters across restarts */
  int nb_iter_tot;		/* nb of iterations total */
  int nb_swap_tot;		/* nb of swaps total */
  int nb_same_var_tot;		/* nb of vars with highest cost total */
  int nb_reset_tot;		/* nb of resets total */
  int nb_local_min_tot;		/* nb of local mins total */


				/* --- other values (e.g. from main) not used by the solver engine --- */

  int param;			/* command-line integer parameter */
  char param_file[512];         /* command-line file name parameter */
  int seed;			/* random seed (or -1 if any) */
  int reset_percent;		/* percentage of variables to reset */
  int data32[4];		/* some 32 bits  */
  long long data64[2];		/* some 64 bits  */

} AdData;

#define RESULTS_CHAR_MSG_SIZE 256 /* with \n */

/*------------------*
 * Global variables *
 *------------------*/

int *ad_sol ALIGN;		/* copy of p_ad->sol (used by no_cost_swap) */
int ad_reinit_after_if_swap;	/* copy of p_ad->reinit_after_if_swap (used by no_cost_swap) */

int ad_no_cost_var_fct;		/* true if a user Cost_On_Variable is not defined */
int ad_no_displ_sol_fct;	/* true if a user Display_Sol is not defined */

#if defined(AD_SOLVER_FILE) && defined(DEBUG) && (DEBUG & 32)
int ad_has_debug = 1;
#else
int ad_has_debug;
#endif

#if defined(AD_SOLVER_FILE) && defined(LOG_FILE)
int ad_has_log_file = 1;
#else
int ad_has_log_file;
#endif

#if defined PRINT_COSTS
int nb_digits_nbprocs ;             /* for exple, 128 procs -> 3 */
int file_descriptor_print_cost ;
#endif

int my_num ;                        /* used for MPI and seq code */

/*------------*
 * Prototypes *
 *------------*/

#if defined PRINT_COSTS
void
print_costs() ;
#endif

int Ad_Solve(AdData *p_ad);

void Ad_Swap(int i, int j);

void Ad_Un_Mark(int i);

void Ad_Display(int *t, AdData *p_ad, unsigned *mark);

/* functions provided by the user */

int Cost_Of_Solution(int should_be_recorded);		/* mandatory */

int Cost_On_Variable(int i);				/* optional else exhaustive search */

int Cost_If_Swap(int current_cost, int i, int j);	/* optional else use Cost_Of_Solution */

void Executed_Swap(int i, int j); 			/* optional else use Cost_Of_Solution */

int Next_I(int i);					/* optional else from 0 to p_ad->size-1 */

int Next_J(int i, int j);				/* optional else from i+1 to p_ad->size-1 */

int Reset(int nb_to_reset, AdData *p_ad);		/* optional else random reset */

void Display_Solution(AdData *p_ad);			/* optional else basic display */

#if 0
#define IGNORE_MARK_IF_BEST
#endif


  /* what to do with marked vars at reset: 
   * 0=nothing, 1=unmark reset (swapped) vars, 2=unmark all vars 
   */

//#define UNMARK_AT_RESET  0
//#define UNMARK_AT_RESET  1
#define UNMARK_AT_RESET  2


#endif /* !AD_SOLVER_H */
