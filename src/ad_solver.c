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

#ifdef PRINT_COSTS /* Print a sum up of (cost;iter) to picture the curve */
int vec_costs[500000] ;
unsigned long int card_vec_costs ;
#endif

#if defined MPI
#include <math.h>  /* ceil() */
char * protocole_name [LS_NBMSGS] =
  {
    "Killall",
    "Sending results",
    "Sending cost"
  } ;
#endif /* MPI */


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




/*------------*
 * Prototypes *
 *------------*/

#if defined(DEBUG) && (DEBUG&1)
static void Show_Debug_Info(void);
#endif

#if !(defined MPI)

#undef DPRINTF

#if defined(DEBUG) && (DEBUG & 4)
#define DPRINTF(...) printf (__VA_ARGS__)
#else
#define DPRINTF(...) ((void) 0)
#endif

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

#ifdef MPI
void
dead_end_final()
{
  int flag = 0 ;          /* .. if operation completed */
#ifdef DEBUG_MPI_ENDING
  struct timeval tv ;
#endif

  /*  printf("MPI_Finalize() sans free\n") ;
  MPI_Finalize() ;
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize()\n",
           tv.tv_sec, tv.tv_usec, my_num) ;
  free_data_of_ad_solver() ;
  return ;
  */

  /* free the_message */
  free( the_message ) ;
  /* Cancel pending messages */
  while( list_recv_msgs.next != NULL ) {
    the_message = get_tegami_from( &list_recv_msgs) ;
    /* Already recvd... and treated */
    MPI_Test( &(the_message->handle), &flag, MPI_STATUS_IGNORE ) ;
    if( flag != 1 ) {
#ifdef DEBUG_MPI_ENDING
    gettimeofday(&tv, NULL);
    DPRINTF("%ld.%ld: %d launches MPI_Cancel()\n",
	    tv.tv_sec, tv.tv_usec, my_num) ;
#endif
      MPI_Cancel( &(the_message->handle) ) ;
      MPI_Wait( &(the_message->handle), MPI_STATUS_IGNORE ) ;
#ifdef DEBUG_MPI_ENDING
      gettimeofday(&tv, NULL);
      DPRINTF("%ld.%ld: %d finished waiting of canceling\n",
	      tv.tv_sec, tv.tv_usec, my_num) ;
#endif

    }
    /*    free( the_message ) ; */
  }

  /*  MPI_Finalize() ;
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize()\n",
           tv.tv_sec, tv.tv_usec, my_num) ;
  return ;
  */
  while( list_allocated_msgs.next != NULL ) {
    the_message = get_tegami_from( &list_allocated_msgs) ;
    free( the_message ) ;
  }
  while( list_sent_msgs.next != NULL ) {
    the_message = get_tegami_from( &list_sent_msgs) ;
    /*    MPI_Cancel( &(the_message->handle) ) ;
	  MPI_Wait( &(the_message->handle), MPI_STATUS_IGNORE ) ;
    */
    free( the_message ) ;
  }
  /* free all remainding structures */
  /* Arimasu ka? */
  
#ifdef DEBUG_MPI_ENDING
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize()...\n",
	 tv.tv_sec, tv.tv_usec, my_num) ;
  MPI_Finalize() ;
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize() done\n",
	 tv.tv_sec, tv.tv_usec, my_num) ;
#endif
  /*
    date_end = MPI_Wtime() ;
    DPRINTF("Elapsed time inside MPI is %f\n",date_end-date_begin);
  */
}

/* msg must be same type than message in ad_solver.h */
/* msg of size SIZE_MESSAGE */
void send_log_n( unsigned int * msg, protocol_msg tag_mpi )
{
  unsigned int i, range ;
  unsigned int nb_steps ;
  tegami * message ;
  unsigned int destination_node ;

#ifdef YC_DEBUG
  unsigned int sentNodes[NBMAXSTEPS] ; /* In fact, Log2(n)! */
#endif
#ifdef YC_DEBUG_MPI
  struct timeval tv ;
#endif

  /* TODO: We should try to aggregate some sent_msg */

  /* Copy */
  range = msg[0] ;

  /* Use Log(n) algo */
  nb_steps = ceil(log2(range)) ;

#ifdef YC_DEBUG_MPI
  gettimeofday(&tv, NULL);
  switch( tag_mpi ) {
  case LS_KILLALL:
    DPRINTF("%ld:%ld: Proc %d sending \"%s\" with "
	    "Range %d ; NB_steps %d ; vainqueur %d\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    protocole_name[tag_mpi],
	    range,
	    nb_steps,
	    msg[1]) ;
    break ;
  case LS_COST:
    DPRINTF("%ld:%ld: Proc %d sending \"%s\" with "
	    "Range %d ; NB_steps %d ; cost %d\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    protocole_name[tag_mpi],
	    range,
	    nb_steps,
	    msg[1]) ;
    break ;
  default:
    printf("Undefined !") ;
  }
#endif

  for( i=1 ; i<= nb_steps ; i++ ) {
    message = get_tegami_from( &list_allocated_msgs) ;

#ifdef ITER_COST
    /* #iter */
    message->message[2] = msg[2] ;
#endif
    /* Winner */    
    message->message[1] = msg[1] ;
    /* Range that we'll send */
    message->message[0] = floor(range/2.0) ;
    /* Send to */
    destination_node = (my_num + (int)ceil(range/2.0)) % mpi_size ;
    /* For next iteration */
    range = range - message->message[0] ;

#ifdef YC_DEBUG_MPI
#ifdef COMM_COST
    /* Send to ... */
    gettimeofday(&tv, NULL);
    sentNodes[i-1] = destination_node ;
    DPRINTF("%ld:%ld: Proc %d sends msg %d protocol %s range %d to %d !\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    message->message[1],
	    protocole_name[tag_mpi],
	    message->message[0],
	    sentNodes[i-1]) ;
#endif
#ifdef ITER_COST
    /* Send to ... */
    gettimeofday(&tv, NULL);
    sentNodes[i-1] = destination_node ;
    DPRINTF("%ld:%ld: Proc %d sends msg (%d;%d) protocol %s range %d to %d !\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    message->message[1],
	    message->message[2],
	    protocole_name[tag_mpi],
	    message->message[0],
	    sentNodes[i-1]) ;
#endif
#endif
    /* Sends */
    MPI_Isend(message->message, SIZE_MESSAGE, MPI_INT,
	      destination_node,
	      tag_mpi, MPI_COMM_WORLD, &(message->handle)) ;
    push_tegami_on( message, &list_sent_msgs) ;
  }
#ifdef YC_DEBUG
  DPRINTF("-- %d sent to [", my_num) ;
  for( i=0 ; i< nb_steps ; i++ )
    DPRINTF(" %d ", sentNodes[i] ) ;
  DPRINTF("] \n" ) ;
#endif
}
#endif /* MPI */

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
#define Emit_Log(...)					\
  do if (f_log)						\
    {							\
      fprintf(f_log, __VA_ARGS__);			\
      fputc('\n', f_log);				\
      fflush(f_log);					\
  } while(0)
#else
#define Emit_Log(...)
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

#ifdef MPI
#if (defined(YC_DEBUG))||(defined(YC_DEBUG_MPI))||(defined(YC_DEBUG_RESTART))
  struct timeval tv ;
#endif
  unsigned int i ;
  int flag = 0 ;          /* .. if operation completed */
  char results[RESULTS_CHAR_MSG_SIZE] ;
  /*  protocol_msg mixed_received_msg_protocol = LS_NBMSGS ; */
  unsigned int number_received_msgs ; /* # of received msgs in a block of iteration */
  unsigned int nb_block = 0 ;             /* Number of C_iter */
  tegami * tmp_tegami ;
#if (defined COMM_COST) || (defined ITER_COST)
  unsigned int mixed_received_msg_cost ; /* Store the min of the recv cost */
  unsigned int best_cost_sent = INT_MAX ; /* The last cost that we sent */
  unsigned int best_cost_received = INT_MAX ; /* The last cost that we received */
  unsigned int s_cost_message[SIZE_MESSAGE] ; /* COMM_COST (range ; cost) */
                                           /* ITER_COST (range ; cost ; iter) */
  unsigned int ran_tmp ;
#endif /* COMM_COST || ITER_COST */
#ifdef ITER_COST
  int iter_of_best_cost_sent = INT_MAX ; /* #iter of best cost sent */
  int mixed_received_msg_iter = -1 ;
  int iter_for_best_cost_received ; /* unused for the moment */
#endif /* ITER_COST */
#ifdef MIN_ITER_BEFORE_RESTART
  unsigned int nbiter_since_restart = 0 ;
#endif
#endif /* MPI */

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

#ifdef PRINT_COSTS
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

#ifdef PRINT_COSTS
      card_vec_costs++ ;
      vec_costs[card_vec_costs] = ad.total_cost ;
#endif
      
#ifdef MPI
      if( (ad.nb_iter % count_to_communication)==0 ) {
	nb_block++ ;
	/* Try to free older messages in their sending order */
#ifdef YC_DEBUG
	gettimeofday(&tv, NULL);
	DPRINTF("%ld:%ld --------------- %d,%d: Free older messages\n", 
		tv.tv_sec, tv.tv_usec,
		my_num,
		nb_block) ;
#endif /* YC_DEBUG */
	flag = 1 ;
	while( (list_sent_msgs.next != NULL) && (flag!=0) ) {
#ifdef YC_DEBUG_MPI
	  gettimeofday(&tv, NULL);
	  DPRINTF("%ld:%ld: Proc %d launches MPI_TEST()\n",
		  tv.tv_sec, tv.tv_usec,
		  my_num) ;
#endif /* YC_DEBUG_MPI */
	  MPI_Test( &((list_sent_msgs.previous)->handle), &flag,
		    MPI_STATUS_IGNORE ) ;
	  if( flag != 0 ) {
	    push_tegami_on( unqueue_tegami_from(&list_sent_msgs),
			    &list_allocated_msgs) ;
	  } /* if( flag != 0 ) { */
	} /* while( (list_sent_msgs.next != NULL) && (flag!=0) ) { */
#ifdef YC_DEBUG
	DPRINTF("-------------- %d: Reception\n", my_num) ;
#endif	
	/*-------------------------------------------------------------*/
	/*************************** Reception *************************/
	/*-------------------------------------------------------------*/
	/************** Check how many msg were received ***************/
	number_received_msgs = 0 ;
#if (defined COMM_COST) || (defined ITER_COST)
	mixed_received_msg_cost = INT_MAX ;
#endif
	do {
#ifdef YC_DEBUG_MPI
	  gettimeofday(&tv, NULL);
	  DPRINTF("%ld:%ld: Proc %d launches MPI_TEST()\n",
		  tv.tv_sec, tv.tv_usec,
		  my_num) ;
#endif
	  MPI_Test(&(the_message->handle), &flag, &(the_message->status)) ;
	  if( flag > 0 ) {                     /* We received at least one! */
#ifdef YC_DEBUG_MPI
	    gettimeofday(&tv, NULL);
	    DPRINTF("%ld:%ld: %d received protocol %s from %d\n",
		    tv.tv_sec, tv.tv_usec,
		    my_num,
		    protocole_name[the_message->status.MPI_TAG],
		    the_message->status.MPI_SOURCE) ;
#endif
	    /* Initiate treatment of all msg */
	    number_received_msgs++ ;
	    switch( the_message->status.MPI_TAG ) {
	      /******************************* LS_KILLALL *********************/
	    case LS_KILLALL:
	      /* Then quits! */
#ifdef YC_DEBUG
	      DPRINTF("LS_KILLALL from %d\n",
		      the_message->status.MPI_SOURCE) ;
#endif /* YC_DEBUG */
	      if( my_num == 0 ) {
		/* Reuse msg and kill everyone */
		the_message->message[0] = mpi_size ;
		the_message->message[1] = the_message->status.MPI_SOURCE ;
		send_log_n( the_message->message, LS_KILLALL ) ;
		/* Recv results from source */
#ifdef YC_DEBUG_MPI
		gettimeofday(&tv, NULL);
		DPRINTF("%ld.%ld: %d launches MPI_Irecv() of results for"
			" source %d\n",
			tv.tv_sec, tv.tv_usec, my_num,
			the_message->status.MPI_SOURCE) ;
#endif
		MPI_Recv( results, RESULTS_CHAR_MSG_SIZE, MPI_CHAR,
			  the_message->status.MPI_SOURCE, SENDING_RESULTS,
			  MPI_COMM_WORLD,
			  MPI_STATUS_IGNORE) ;
		printf("%s\n", results) ;
#ifdef PRINT_COSTS
		print_costs() ;
		printf("*** 0 launches MPI_Abort() in %d secs\n", mpi_size*3);
		sleep(mpi_size*3) ;
#endif
		printf("*** 0 launches MPI_Abort()\n");
		MPI_Abort(MPI_COMM_WORLD, my_num) ;
		/* dead_end_final() ; */
		exit(0) ;
	      } else {                                  /* Proc N */
		/* S.o finished before me. I killall the ones I'm responsible */
		send_log_n( the_message->message, LS_KILLALL ) ;
#ifdef PRINT_COSTS
		print_costs() ;
#endif
		/* Wait for proc 0 to call MPI_Abort() */
		sleep(mpi_size*4) ;
		DPRINTF("*** %d should never print this:"
			" means that proc 0 spent more than %d"
			" sec to call MPI_Abort()\n",
			my_num, mpi_size*4);
	      }
	      break ;
#if (defined COMM_COST) || (defined ITER_COST)
	      /******************************* LS_COST *********************/
	    case LS_COST:     /* take the min! */
#ifdef YC_DEBUG
	      DPRINTF("%d takes the min between min'_recv(%d) and recvd(%d)\n",
		      my_num,
		      mixed_received_msg_cost, the_message->message[1]) ;
#endif
	      if( mixed_received_msg_cost > the_message->message[1] ) {
		mixed_received_msg_cost = the_message->message[1] ;
#ifdef ITER_COST
		/* Save according iter */
		mixed_received_msg_iter = the_message->message[2] ;
#endif
	      }
	      /* Store recvd msg to treat it later */
	      push_tegami_on( the_message, &list_recv_msgs) ;
	      /* Launch new async recv */
	      the_message = get_tegami_from( &list_allocated_msgs) ;
#ifdef YC_DEBUG_MPI
	      gettimeofday(&tv, NULL);
	      DPRINTF("%ld.%ld: %d launches MPI_Irecv(), any source\n",
		      tv.tv_sec, tv.tv_usec, my_num) ;
#endif
	      MPI_Irecv(&(the_message->message), SIZE_MESSAGE, MPI_INT,
			MPI_ANY_SOURCE, 
			MPI_ANY_TAG,
			MPI_COMM_WORLD, &(the_message->handle)) ;
	      break ;
#endif /* COMM_COST || ITER_COST */
	    case SENDING_RESULTS:
	    default:
	      printf("Should never happen! Exiting.\n") ;
	      exit(-1) ;
	    } /*  switch( the_message->status.MPI_TAG ) { */
	  } /* if flag */
	} while( flag > 0 ) ;
#ifdef YC_DEBUG
	DPRINTF("%d received %d messages in total this time\n",
		my_num,
		number_received_msgs) ;
	DPRINTF("-------------- %d: Treatment\n", my_num) ;
#endif	
	/****************** Treat all received msgs (except LS_KILLALL) **/
	for( i=0 ; i<number_received_msgs ; i++ ) {
	  tmp_tegami = get_tegami_from( &list_recv_msgs) ;
	  switch( tmp_tegami->status.MPI_TAG ) {
#if (defined COMM_COST) || (defined ITER_COST)
	    /******************************* LS_COST *********************/
	  case LS_COST:
#ifdef YC_DEBUG_MPI
	    gettimeofday(&tv, NULL);
	    DPRINTF("%ld:%ld: %d treats LS_COST of %d from %d\n",
		    tv.tv_sec, tv.tv_usec,
		    my_num,
		    tmp_tegami->message[1],
		    tmp_tegami->status.MPI_SOURCE) ;
#endif /* YC_DEBUG */
	    /* Crash cost with our value and avoids a test */
	    tmp_tegami->message[1] = mixed_received_msg_cost ;
#ifdef ITER_COST
	    tmp_tegami->message[2] = mixed_received_msg_iter ;
#endif
	    /* [cont. to] Distribute the information */
	    send_log_n( tmp_tegami->message, LS_COST ) ;
	    /* Msg received and treated */
	    push_tegami_on( tmp_tegami, &list_allocated_msgs) ;
	    break;
#endif /* COMM_COST || ITER_COST */
	  default:
	    printf("This should never happen! Exiting...\n") ;
	    exit(-1) ;
	  } /* switch( the_message->status.MPI_TAG ) { */
	} /* for( i=0 ; i<number_received_msgs ; i++ ) { */
#ifdef YC_DEBUG
	DPRINTF("-------------- %d: Impact\n", my_num) ;
#endif	
	/****************** Repercusion of messages on me *************/
#if (defined COMM_COST) || (defined ITER_COST)
	if( number_received_msgs > 0 ) {
	  /** Do we take into account the received cost? **/
	  if( mixed_received_msg_cost < best_cost_received ) {
	    best_cost_received = mixed_received_msg_cost ;
#ifdef ITER_COST
	    iter_for_best_cost_received = mixed_received_msg_iter ;
#endif
	  }
#ifdef YC_DEBUG
	  DPRINTF("%d: Best recv cost is now %d\n",
		  my_num, best_cost_received) ;
#ifdef ITER_COST
	  DPRINTF("... with #iter %d\n", iter_for_best_cost_received ) ;
#endif
#endif
	  /******************************* COMM_COST ****************/
#ifdef COMM_COST
	  if( (unsigned)ad.total_cost > mixed_received_msg_cost ) {
	    ran_tmp=(((float)rand())/RAND_MAX)*100 ;
#ifdef YC_DEBUG	  
	    DPRINTF("Proc %d (cost %d > %d): ran=%d >?< %d\n",
		    my_num,
		    ad.total_cost,
		    mixed_received_msg_cost,
		    ran_tmp,
		    proba_communication) ;
#endif

	    if( ran_tmp < proba_communication ) {
#ifdef YC_DEBUG_RESTART
	      gettimeofday(&tv, NULL);
	      DPRINTF("%ld:%ld: Proc %d restarts!\n",
		      tv.tv_sec, tv.tv_usec,
		      my_num) ;
#endif
	      goto restart ;
	    } /* if( tan_tmp */  
	  } /* if( ad.total.cost */
#endif /* COMM_COST */
	  /******************************* ITER_COST ****************/
#ifdef ITER_COST
	  /* Best cost AND smaller #iter */
	  if( (unsigned)ad.total_cost > mixed_received_msg_cost ) {
	    if( mixed_received_msg_iter < ad.nb_iter ) {
	      
	      ran_tmp=(((float)rand())/RAND_MAX)*100 ;
#ifdef YC_DEBUG	  
	      DPRINTF("Proc %d (cost %d > %d): ran=%d >?< %d\n",
		      my_num,
		      ad.total_cost,
		      mixed_received_msg_cost,
		      ran_tmp,
		      proba_communication) ;
#endif
	      if( ran_tmp < proba_communication ) {
#ifdef YC_DEBUG_RESTART
		gettimeofday(&tv, NULL);
		DPRINTF("%ld:%ld: Proc %d restarts!\n",
			tv.tv_sec, tv.tv_usec,
			my_num) ;
#endif
		goto restart ;
	      } /* if( tan_tmp */  
	    }/* if( mixed_recv */
	  } /* if( ad.total.cost */
#endif /* ITER_COST */
	} /* if( number_received_msgs > 0 ) { */
#endif /* COMM_COST || ITER_COST */
#ifdef YC_DEBUG
	DPRINTF("-------------- %d: Sending\n", my_num) ;
#endif	
	/*-------------------------------------------------------------*/
	/**************************** Sending **************************/
	/*-------------------------------------------------------------*/
#if (defined COMM_COST) || (defined ITER_COST)
	/************** Sends best cost? ************/
	if( (unsigned)best_cost < best_cost_sent ) {
#ifdef YC_DEBUG
	  DPRINTF("Proc %d sends cost %d\n", my_num, best_cost) ;
#endif /* YC_DEBUG */
	  s_cost_message[0] = mpi_size ;
	  s_cost_message[1] = best_cost ;
#ifdef ITER_COST
	  s_cost_message[1] = ad.nb_iter ;
#endif
	  send_log_n(s_cost_message, LS_COST) ;	      
	  best_cost_sent = best_cost ;
#ifdef ITER_COST
	  iter_of_best_cost_sent = ad.nb_iter ;
#endif
	} /* if( best_cost < best_cost_sent ) { */
#endif /* COMM_COST || ITER_COST */
      } /* if( (ad.nb_iter % count_to_communication)==0 ) { */
#endif /* MPI */


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


