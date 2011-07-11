/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  main.c: benchmark main function
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>                     /* gettimeofday() */

#if defined PRINT_COSTS
#include <unistd.h>                       /* sleep() */
#include <sys/types.h>                    /* open()... */
#include <sys/stat.h>                     /* and S_IRUSR... */
#include <fcntl.h>                        /* and O_WDONLY */
#include <string.h>                       /* strdup() */
#endif

#include "main_MPI.h"
#include "ad_solver_MPI.h"
#include "ad_solver.h"
#include "tools.h"                         /* PRINT, DPRINT, TPRINT, TDPRINT */

/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/

/*------------------*
 * Global variables *
 *------------------*/

int nb_threads;			/* must be ld-global! */

static int count;

static int disp_mode;
static int check_valid;
static int read_initial;	/* 0=no, 1=yes, 2=all threads use the same (CELL specific) */


int param_needed;		/* overwritten by benches if an argument is needed (> 0 = integer, < 0 = file name) */
char *user_stat_name;		/* overwritten by benches if a user statistics is needed */
int (*user_stat_fct)(AdData *p_ad); /* overwritten by benches if a user statistics is needed */

#if defined PRINT_COSTS
  char * filename_pattern_print_cost ;
#endif

/*------------*
 * Prototypes *
 *------------*/

static void Set_Initial(AdData *p_ad);

static void Verify_Sol(AdData *p_ad);

static void Parse_Cmd_Line(int argc, char *argv[], AdData *p_ad);

static void PrintAllCompilationOptions() ; /* MPI or not */

#define Div_Round_Up(x, y)   (((x) + (y) - 1) / (y))

/* provided by each bench */

void Init_Parameters(AdData *p_ad);

int Check_Solution(AdData *p_ad);

#ifdef CELL

#define User_Time    Real_Time
#define Solve(p_ad)  SolveStub(p_ad)

void SolveStub(AdData *p_ad);

#else  /* !CELL */

void Solve(AdData *p_ad);

#endif	/* !CELL */

/*
 *  MAIN
 *
 */

int
main(int argc, char *argv[])
{
  static AdData data;		/* to be init with 0 (debug only) */
  AdData *p_ad = &data;
  int i, user_stat = 0;

  double time_one0, time_one;
  double nb_same_var_by_iter, nb_same_var_by_iter_tot;

  int    nb_iter_cum;
  int    nb_local_min_cum;
  int    nb_swap_cum;
  int    nb_reset_cum;
  double nb_same_var_by_iter_cum;


  int    nb_restart_cum,           nb_restart_min,              nb_restart_max;
  double time_cum,                    time_min,                    time_max;

  int    nb_iter_tot_cum,          nb_iter_tot_min,             nb_iter_tot_max;
  int    nb_local_min_tot_cum,     nb_local_min_tot_min,   nb_local_min_tot_max;
  int    nb_swap_tot_cum,          nb_swap_tot_min,             nb_swap_tot_max;
  int    nb_reset_tot_cum,         nb_reset_tot_min,          nb_reset_tot_max;
  double nb_same_var_by_iter_tot_cum, nb_same_var_by_iter_tot_min, nb_same_var_by_iter_tot_max;

  int    user_stat_cum,             user_stat_min,               user_stat_max;
  char buff[256], str[32];

  /* Seeds generation */
  int last_value;	/* last value generated by the linear chaotic map */
  int param_a;		/* parameter 'a' for the linear chaotic map */
  int param_c;		/* parameter 'c' for the linear chaotic map */
  long int print_seed ;
  struct timeval tv ;
#if defined PRINT_COSTS
  char * tmp_filename=NULL ;
#endif /* PRINT_COSTS */
#if defined MPI
  Main_MPIData mpi_data ;
#endif

  Parse_Cmd_Line(argc, argv, p_ad);

  /************************ Initialize chaotic function **********************/
  param_a = 5;                      /* Values by default from research paper */
  param_c = 1;
  gettimeofday(&tv, NULL);

  /*********************** MPI & SEQ code Initialization *********************/
#if defined MPI
  mpi_data.param_a_ptr = &param_a ;
  mpi_data.param_c_ptr = &param_c ;
  mpi_data.last_value_ptr = &last_value ;
  mpi_data.p_ad = p_ad ;
  mpi_data.p_ad->main_mpi_data_ptr = &mpi_data ;
  mpi_data.print_seed_ptr = &print_seed ;
  mpi_data.tv_sec = tv.tv_sec ;
  mpi_data.count_ptr = &count ;

#if defined STATS
  Gmpi_stats.nb_sent_messages = 0 ;
  Gmpi_stats.nb_sent_mymessages = 0 ;
#endif

#if defined PRINT_COSTS
  mpi_data.nb_digits_nbprocs_ptr = &nb_digits_nbprocs ;
#endif
  MPI_Init( &argc , &argv ) ;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_num) ;
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size) ;
  TPRINT0("Program: %s", argv[0]) ;
  for( i=1 ; i< argc ; ++i )
    PRINT0(" %s", argv[i]) ;
  PRINT0("\n") ;
  AS_MPI_initialization( &mpi_data ) ;
#else /****************************** MPI -> SEQ *****************************/
  TPRINT0("Program: %s", argv[0]) ;
  for( i=1 ; i< argc ; i++ )
    PRINT0(" %s", argv[i]) ;
  PRINT0("\n") ;
#if defined PRINT_COSTS
  nb_digits_nbprocs = 0 ;
#endif /* PRINT_COSTS */
  if (p_ad->seed < 0) {
    srandom((unsigned int)tv.tv_sec);
    /* INT_MAX / 6 (= 357.913.941) is a reasonable value to start with... 
       I think... */
    last_value = (int) Random(INT_MAX / 6);
    /* INT_MAX (= 2.147.483.647) is the max value for a signed 32-bit int */
    p_ad->seed = randChaos(INT_MAX, &last_value, &param_a, &param_c);
  }
  print_seed = p_ad->seed;
#endif /**************************** MPI */

#if defined PRINT_COSTS
  if( filename_pattern_print_cost==NULL ) {
    PRINT0("Please give a pattern for filename in which to save costs\n\n") ;
    exit(-1) ;
  }
  tmp_filename=(char*)
    malloc(sizeof(char)*strlen(filename_pattern_print_cost) 
	   + 2
	   + nb_digits_nbprocs ) ;
  if( nb_digits_nbprocs > 3 ) {
    PRINT0("You use a number of procs sup to 999. "
	   "You must modify the code to adjust next line\n") ;
    exit(-1) ;
  }
#if defined MPI
  /* TODO: How can we bypass the static char to format output in nxt line? */
  sprintf(tmp_filename,"%s_p%03d", filename_pattern_print_cost,my_num) ;
#else
  sprintf(tmp_filename,"%s_seq", filename_pattern_print_cost) ;
#endif
  file_descriptor_print_cost = open(tmp_filename,
				    O_WRONLY | O_EXCL | O_CREAT,
				    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) ;
  if( file_descriptor_print_cost == -1 ) {
    PRINTF("Cannot create file to print costs: file already exists?\n") ;
    return -1 ;
  }
#endif /* PRINT_COSTS */

  /********* Initialization of the pseudo-random generator with the seed ******/
  srandom((unsigned)p_ad->seed);

  p_ad->nb_var_to_reset = -1;
  p_ad->do_not_init = 0;
  p_ad->actual_value = NULL;
  p_ad->base_value = 0;
  p_ad->break_nl = 0;
  /* defaults */

  Init_Parameters(p_ad);

  if (p_ad->reset_limit >= p_ad->size)
    p_ad->reset_limit = p_ad->size - 1;

  setvbuf(stdout, NULL, _IOLBF, 0);
  //setlinebuf(stdout);

  if (p_ad->debug > 0 && !ad_has_debug)
    DPRINT0("Warning ad_solver is not compiled with debugging support\n") ;

  if (p_ad->log_file && !ad_has_log_file)
    DPRINT0("Warning ad_solver is not compiled with log file support\n") ;

  p_ad->size_in_bytes = p_ad->size * sizeof(int);
  p_ad->sol = malloc(p_ad->size_in_bytes);

  if (p_ad->nb_var_to_reset == -1) {
    p_ad->nb_var_to_reset = Div_Round_Up(p_ad->size * p_ad->reset_percent, 100);
    if (p_ad->nb_var_to_reset < 2) {
      p_ad->nb_var_to_reset = 2;
      PRINT0("increasing nb var to reset since too small, now = %d\n",
	     p_ad->nb_var_to_reset);
    }
  }

#if defined MPI
  AS_MPI_initialization_epilogue( &mpi_data ) ;
#endif

  /********** Print configuration information + specific initialization *****/  
  PRINT0("current random seed used: %u (seed_0 %u) \n",
	 (unsigned int)p_ad->seed, (unsigned int)print_seed) ;
  PRINT0("variables of loc min are frozen for: %d swaps\n",
	 p_ad->freeze_loc_min) ;
  PRINT0("variables swapped are frozen for: %d swaps\n", p_ad->freeze_swap) ;
  if (p_ad->reset_percent >= 0)
    PRINT0("%d %% = ", p_ad->reset_percent) ;
  PRINT0("%d variables are reset when %d variables are frozen\n", 
	 p_ad->nb_var_to_reset, p_ad->reset_limit) ;
  PRINT0("probability to select a local min (instead of "
	 "staying on a plateau): ") ;
  if (p_ad->prob_select_loc_min >=0 && p_ad->prob_select_loc_min <= 100)
    PRINT0("%d %%\n", p_ad->prob_select_loc_min) ;
  else PRINT0("not used\n") ;
  PRINT0("abort when %d iterations are reached "
	 "and restart at most %d times\n",
	 p_ad->restart_limit, p_ad->restart_max) ;

  PrintAllCompilationOptions() ;

#if defined BACKTRACK
  /* Note: This has to be done after p_ad initialization! */
  /* Gbacktrack_array_begin = 0 ; */
  /* Gbacktrack_array_end = 0 ; */
  /* Gconfiguration_size_in_bytes = p_ad->size_in_bytes ; */
  /* for( i=0 ; i<SIZE_BACKTRACK ; i++ ) */
  /*   Gbacktrack_array[i].configuration = (unsigned int*) */
  /*     malloc(Gconfiguration_size_in_bytes) ; */
  /* YC->all: do the rest of initilization */

  gl_elitePool.config_list_begin		= NULL;
  gl_elitePool.config_list_end			= NULL;
  gl_elitePool.config_list_size			= 0;
  gl_elitePool.nb_backtrack			= 0;
  gl_elitePool.nb_variable_backtrack		= 0;
  gl_elitePool.nb_value_backtrack		= 0;

  gl_stockPool.config_list_begin		= NULL;
  gl_stockPool.config_list_end			= NULL;
  gl_stockPool.config_list_size			= 0;

  backtrack_configuration *item;
  for (i = 0; i < SIZE_BACKTRACK; i++)
    {
      item			= malloc(sizeof(backtrack_configuration));
      item->configuration	= malloc(p_ad->size_in_bytes);
      pushStock(item);
    }

#endif

  TPRINT0("%d begins its resolution!\n", my_num) ;
  TPRINT0("count = %d\n", count);


  if (count <= 0) /* Note: MPI => count=1 */
    {
      Set_Initial(p_ad);

      time_one0 = (double) User_Time();
      Solve(p_ad);
      time_one = ((double) User_Time() - time_one0) / 1000;

      if (p_ad->exhaustive)
	DPRINTF("exhaustive search\n") ;
      
      if (count < 0)
	Display_Solution(p_ad);

      Verify_Sol(p_ad);

      if (p_ad->total_cost)
	PRINTF("*** NOT SOLVED (cost of this pseudo-solution: %d) ***\n", p_ad->total_cost) ;

      if (count == 0)
	{
	  nb_same_var_by_iter = (double) p_ad->nb_same_var / p_ad->nb_iter;
	  nb_same_var_by_iter_tot = (double) p_ad->nb_same_var_tot / p_ad->nb_iter_tot;

	  PRINTF("%5d %8.2f %8d %8d %8d %8d %8.1f %8d %8d %8d %8d %8.1f", 
		 p_ad->nb_restart, time_one, 
		 p_ad->nb_iter, p_ad->nb_local_min, p_ad->nb_swap, 
		 p_ad->nb_reset, nb_same_var_by_iter,
		 p_ad->nb_iter_tot, p_ad->nb_local_min_tot, p_ad->nb_swap_tot, 
		 p_ad->nb_reset_tot, nb_same_var_by_iter_tot);
	  if (user_stat_fct)
	    PRINTF(" %8d", (*user_stat_fct)(p_ad));
	  PRINTF("\n");
	}
      else
	{
	  PRINTF("in %.2f secs (%d restarts, %d iters, %d loc min, %d swaps, %d resets)\n", 
		 time_one, p_ad->nb_restart, p_ad->nb_iter_tot, p_ad->nb_local_min_tot, 
		 p_ad->nb_swap_tot, p_ad->nb_reset_tot);
	}
#if defined BACKTRACK
      PRINTF("BACKTRACK STATS:\n");
      PRINTF("Total number of performed backtracks: %d\n", gl_elitePool.nb_backtrack);
      PRINTF("Total number of performed backtracks starting from another variable: %d\n", gl_elitePool.nb_variable_backtrack);
      PRINTF("Total number of performed backtracks starting from another value: %d\n", gl_elitePool.nb_value_backtrack);
#endif

      return 0 ;
    } /* (count <= 0) */

  if (user_stat_name)
    sprintf(str, " %8s |", user_stat_name);
  else
    *str = '\0';

  snprintf(buff, 255,
	   "|Count|restart|     time |    iters |  loc min |    swaps "
	   "|   resets | same/iter|%s\n", str);

  if (param_needed > 0)
    PRINT0("%*d\n", (int) strlen(buff)/2, p_ad->param) ;
  else if (param_needed < 0)
    PRINT0("%*s\n", (int) strlen(buff)/2, p_ad->param_file) ;

  PRINT0("%s", buff) ;
  for(i = 0; buff[i] != '\n'; ++i)
    if (buff[i] != '|')
      buff[i] = '-';
  PRINT0("%s\n", buff) ;

  nb_restart_cum = time_cum = user_stat_cum = 0;

  nb_iter_cum = nb_local_min_cum = nb_swap_cum = nb_reset_cum = 0;
  nb_same_var_by_iter_cum = user_stat_cum = 0;


  nb_iter_tot_cum = nb_local_min_tot_cum = nb_swap_tot_cum = nb_reset_tot_cum = 0;
  nb_same_var_by_iter_tot_cum = 0;

  nb_restart_min = user_stat_min = (1 << 30);
  time_min = 1e100;
  
  nb_iter_tot_min = nb_local_min_tot_min = nb_swap_tot_min = nb_reset_tot_min = (1 << 30);
  nb_same_var_by_iter_tot_min = 1e100;

  nb_restart_max = user_stat_max = 0;
  time_max = 0;
 
  nb_iter_tot_max = nb_local_min_tot_max = nb_swap_tot_max = nb_reset_tot_max = 0;
  nb_same_var_by_iter_tot_max = 0;


  for(i = 1; i <= count; i++)
    {
      Set_Initial(p_ad);

      time_one0 = (double) User_Time();
      Solve(p_ad);
      time_one = ((double) User_Time() - time_one0) / 1000;

#if !defined MPI /* Slashes MPI output! */
      if (disp_mode == 2 && nb_restart_cum > 0)
	PRINTF("\033[A\033[K");
      PRINTF("\033[A\033[K\033[A\033[256D");
#endif

      Verify_Sol(p_ad);

      if (user_stat_fct)
	user_stat = (*user_stat_fct)(p_ad);


      nb_same_var_by_iter = (double) p_ad->nb_same_var / p_ad->nb_iter;
      nb_same_var_by_iter_tot = (double) p_ad->nb_same_var_tot / p_ad->nb_iter_tot;

      nb_restart_cum += p_ad->nb_restart;
      time_cum += time_one;
      nb_iter_cum += p_ad->nb_iter;
      nb_local_min_cum += p_ad->nb_local_min;
      nb_swap_cum += p_ad->nb_swap;
      nb_reset_cum += p_ad->nb_reset;
      nb_same_var_by_iter_cum += nb_same_var_by_iter;
      user_stat_cum += user_stat;

      nb_iter_tot_cum += p_ad->nb_iter_tot;
      nb_local_min_tot_cum += p_ad->nb_local_min_tot;
      nb_swap_tot_cum += p_ad->nb_swap_tot;
      nb_reset_tot_cum += p_ad->nb_reset_tot;
      nb_same_var_by_iter_tot_cum += nb_same_var_by_iter_tot;

      if (nb_restart_min > p_ad->nb_restart)
	nb_restart_min = p_ad->nb_restart;
      if (time_min > time_one)
	time_min = time_one;
      if (nb_iter_tot_min > p_ad->nb_iter_tot)
	nb_iter_tot_min = p_ad->nb_iter_tot;
      if (nb_local_min_tot_min > p_ad->nb_local_min_tot)
	nb_local_min_tot_min = p_ad->nb_local_min_tot;
      if (nb_swap_tot_min > p_ad->nb_swap_tot)
	nb_swap_tot_min = p_ad->nb_swap_tot;
      if (nb_reset_tot_min > p_ad->nb_reset_tot)
	nb_reset_tot_min = p_ad->nb_reset_tot;
      if (nb_same_var_by_iter_tot_min > nb_same_var_by_iter_tot)
	nb_same_var_by_iter_tot_min = nb_same_var_by_iter_tot;
      if (user_stat_min > user_stat)
	user_stat_min = user_stat;

      if (nb_restart_max < p_ad->nb_restart)
	nb_restart_max = p_ad->nb_restart;
      if (time_max < time_one)
	time_max = time_one;
      if (nb_iter_tot_max < p_ad->nb_iter_tot)
	nb_iter_tot_max = p_ad->nb_iter_tot;
      if (nb_local_min_tot_max < p_ad->nb_local_min_tot)
	nb_local_min_tot_max = p_ad->nb_local_min_tot;
      if (nb_swap_tot_max < p_ad->nb_swap_tot)
	nb_swap_tot_max = p_ad->nb_swap_tot;
      if (nb_reset_tot_max < p_ad->nb_reset_tot)
	nb_reset_tot_max = p_ad->nb_reset_tot;
      if (nb_same_var_by_iter_tot_max < nb_same_var_by_iter_tot)
	nb_same_var_by_iter_tot_max = nb_same_var_by_iter_tot;
      if (user_stat_max < user_stat)
	user_stat_max = user_stat;
  
#if !defined(MPI)
      switch(disp_mode)
	{
	case 0:			/* only last iter counters */
	case 2:			/* last iter followed by restart if needed */
	  PRINTF("|%4d | %5d%c| %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
		 i, p_ad->nb_restart, (p_ad->total_cost == 0) ? ' ' : 'N',
		 time_one,
		 p_ad->nb_iter, p_ad->nb_local_min, p_ad->nb_swap,
		 p_ad->nb_reset, nb_same_var_by_iter);
	  if (user_stat_fct)
	    PRINTF(" %8d |", user_stat);
	  PRINTF("\n");

	  if (disp_mode == 2 && p_ad->nb_restart > 0) 
	    {
	      PRINTF("|     |       |          |"
		     " %8d | %8d | %8d | %8d | %8.1f |",
		     p_ad->nb_iter_tot, p_ad->nb_local_min_tot,
		     p_ad->nb_swap_tot,
		     p_ad->nb_reset_tot, nb_same_var_by_iter_tot);
	      if (user_stat_fct)
		PRINTF("          |");
	      PRINTF("\n");
	    }

	  PRINTF("%s", buff);

	  PRINTF("| avg | %5d | %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
		 nb_restart_cum / i, time_cum / i,
		 nb_iter_cum / i, nb_local_min_cum / i, nb_swap_cum / i,
		 nb_reset_cum / i, nb_same_var_by_iter_cum / i);
	  if (user_stat_fct)
	    PRINTF(" %8.2f |", (double) user_stat_cum / i);
	  PRINTF("\n");


	  if (disp_mode == 2 && nb_restart_cum > 0) 
	    {
	      PRINTF("|     |       |          |"
		     " %8d | %8d | %8d | %8d | %8.1f |",
		     nb_iter_tot_cum / i, nb_local_min_tot_cum / i,
		     nb_swap_tot_cum / i,
		     nb_reset_tot_cum / i, nb_same_var_by_iter_tot_cum / i);
	      if (user_stat_fct)
		PRINTF("          |");
	      PRINTF("\n");
	    }
	  break;

	case 1:			/* only total (restart + last iter) counters */
	  PRINTF("|%4d | %5d%c| %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
		 i, p_ad->nb_restart, (p_ad->total_cost == 0) ? ' ' : 'N',
		 time_one,
		 p_ad->nb_iter_tot, p_ad->nb_local_min_tot, p_ad->nb_swap_tot,
		 p_ad->nb_reset_tot, nb_same_var_by_iter_tot);
	  if (user_stat_fct)
	    PRINTF(" %8d |", user_stat);
	  PRINTF("\n");

	  PRINTF("%s", buff);

	  PRINTF("| avg | %5d | %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
		 nb_restart_cum / i, time_cum / i,
		 nb_iter_tot_cum / i, nb_local_min_tot_cum / i,
		 nb_swap_tot_cum / i,
		 nb_reset_tot_cum / i, nb_same_var_by_iter_tot_cum / i);
	  if (user_stat_fct)
	    PRINTF(" %8.2f |", (double) user_stat_cum / i);
	  PRINTF("\n");
	  break;
	}
#else /* MPI */
      /* disp_mode equals 1 by default */
      /* Prepare what will be sent to 0, and/or printed by 0 */
      snprintf(mpi_data.results, RESULTS_CHAR_MSG_SIZE - 1,
	       "|* %ld/(%d/%d) | %5d | %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
	       print_seed,
	       my_num,
	       mpi_size,
	       nb_restart_cum / i, time_cum / i,
	       nb_iter_tot_cum / i, nb_local_min_tot_cum / i,
	       nb_swap_tot_cum / i,
	       nb_reset_tot_cum / i, nb_same_var_by_iter_tot_cum / i);
      /* TODO: use if(user_stat_fct)? What is that? */
#endif /* MPI */
    } /* for(i = 1; i <= count; i++) */

  if (count <= 0) /* YC->DD: is it really possible here? return 0 before.*/
    return 0;

  if( count > 1 ) { /* YC->DD: why this test has been removed? */
    PRINTF("| min | %5d | %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
	   nb_restart_min, time_min,
	   nb_iter_tot_min, nb_local_min_tot_min, nb_swap_tot_min,
	   nb_reset_tot_min, nb_same_var_by_iter_tot_min);
    if (user_stat_fct)
      PRINTF(" %8d |", user_stat_min);
    PRINTF("\n");

    PRINTF("| max | %5d | %8.2f | %8d | %8d | %8d | %8d | %8.1f |",
	   nb_restart_max, time_max,
	   nb_iter_tot_max, nb_local_min_tot_max, nb_swap_tot_max,
	   nb_reset_tot_max, nb_same_var_by_iter_tot_max);
    if (user_stat_fct)
      PRINTF(" %8d |", user_stat_max);
    PRINTF("\n");
  }

#if defined BACKTRACK
  PRINTF("BACKTRACK STATS:\n");
  PRINTF("Total number of performed backtracks: %d\n", gl_elitePool.nb_backtrack);
  PRINTF("Total number of performed backtracks starting from another variable: %d\n", gl_elitePool.nb_variable_backtrack);
  PRINTF("Total number of performed backtracks starting from another value: %d\n", gl_elitePool.nb_value_backtrack);

  /* flush pools */
  while(gl_elitePool.config_list_size > 0)
    {
      item = popElite();
      free(item->configuration);
      free(item);
    }
  while(gl_stockPool.config_list_size > 0)
    {
      item = popStock();
      free(item->configuration);
      free(item);
    }
#endif /* BACKTRACK */

#if !( defined MPI )
# if defined PRINT_COSTS
  print_costs() ;
# endif
  /* Seq code is now ending */
  TDPRINTF("Processus ends now.\n") ;
#else /* !( defined MPI) */
  AS_MPI_completion( &mpi_data ) ;
#endif /* MPI */
}



void
Set_Initial(AdData *p_ad)
{
#if defined BACKTRACK
  /* flush the elite pool */
  backtrack_configuration *dummy;
  while (gl_elitePool.config_list_size > 0)
    {
      dummy = popElite();
      pushStock(dummy);
    }
#endif

  int i;
  switch (read_initial)
    {
    case 0:
      break;

    case 1:
      printf("enter the initial configuration:\n");
      for(i = 0; i < p_ad->size; i++)
	if (scanf("%d", &p_ad->sol[i])) /* avoid gcc warning warn_unused_result */
	  {}
      getchar();		/* the last \n */
      Display_Solution(p_ad);
      i = Random_Permut_Check(p_ad->sol, p_ad->size, p_ad->actual_value, p_ad->base_value);
      if (i >= 0)
	{
	  fprintf(stderr, "not a valid permutation, error at [%d] = %d\n",
		 i, p_ad->sol[i]);
	  Random_Permut_Repair(p_ad->sol, p_ad->size, p_ad->actual_value, p_ad->base_value);
	  printf("possible repair:\n");
	  Display_Solution(p_ad);
	  exit(1);
	}
      p_ad->do_not_init = 1;
      break;

    case 2:
      Random_Permut(p_ad->sol, p_ad->size, p_ad->actual_value, p_ad->base_value);
      p_ad->do_not_init = 1;
      break;

#ifdef MPI
    case 3:
      if (my_num == 0)
	{
	  DPRINTF("All processors start with the same configuration!\n");
	  Random_Permut(p_ad->sol, p_ad->size, p_ad->actual_value, p_ad->base_value);
	}
      
      MPI_Bcast(p_ad->sol, p_ad->size, MPI_INT, 0, MPI_COMM_WORLD);
      
# ifdef DEBUG
      printf("Proc %d: ", my_num);
      for (i = 0; i < p_ad->size; i++)
	printf("%4d", p_ad->sol[i]);
      
      printf("\n");
      fflush(stdout);
# endif /* DEBUG */
      
      p_ad->do_not_init = 1;
      break;
#endif /* MPI */      
    }

#if defined(DEBUG) && (DEBUG & 1)
  if (p_ad->do_not_init)
    {
      printf("++++++++++ values to pass to threads (do_not_init=1)\n");
      Display_Solution(p_ad);
      printf("+++++++++++++++++++++++++++\n");
    }
#endif
}




static void
Verify_Sol(AdData *p_ad)
{
  if (p_ad->total_cost != 0 || !check_valid)
    return;

  int i = Random_Permut_Check(p_ad->sol, p_ad->size, p_ad->actual_value, p_ad->base_value);
  if (i >= 0)
    {
      fprintf(stderr, "*** Erroneous Solution !!! not a valid permutation, error at [%d] = %d\n", i, p_ad->sol[i]);
    }
  else
    if (!Check_Solution(p_ad))
      printf("*** Erroneous Solution !!!\n");
}




#define L(msg) fprintf(stderr, msg "\n")


/*
 *  PARSE_CMD_LINE
 *
 */
static void
Parse_Cmd_Line(int argc, char *argv[], AdData *p_ad)
{
  int param_read = 0;
  int i;

  nb_threads = 1;

  count = -1;
  disp_mode = 1;
  check_valid = 0;
  read_initial = 0;

  p_ad->param = -1;
  p_ad->seed = -1;
  p_ad->debug = 0;
  p_ad->log_file = NULL;
  p_ad->prob_select_loc_min = -1;
  p_ad->freeze_loc_min = -1;
  p_ad->freeze_swap = -1;
  p_ad->reset_limit = -1;
  p_ad->reset_percent = -1;
  p_ad->restart_limit = -1;
  p_ad->restart_max = -1;
  p_ad->exhaustive = 0;
  p_ad->first_best = 0;
#ifdef PRINT_COSTS
  filename_pattern_print_cost=NULL ;
#endif
#if defined MPI
  count_to_communication = CBLOCK_DEFAULT ;
#if (defined COMM_COST)||(defined ITER_COST)||(defined COMM_CONFIG)
  proba_communication = 0 ;
#endif
#endif /* MPI */

  for(i = 1; i < argc; ++i)
    {
      if (argv[i][0] == '-')
	{
	  switch(argv[i][1])
	    {
	    case 'i':
	      read_initial = 1;
	      continue;

	    case 'D':
	      if (++i >= argc)
		{
		  L("debug level expected");
		  exit(1);
		}
	      p_ad->debug = atoi(argv[i]);
	      continue;

	    case 's':
	      if (++i >= argc)
		{
		  L("random seed expected");
		  exit(1);
		}
	      p_ad->seed = atoi(argv[i]);
	      continue;

	    case 'L':
	      if (++i >= argc)
		{
		  L("log file name expected");
		  exit(1);
		}
	      p_ad->log_file = argv[i];
	      continue;

	    case 'c':
	      check_valid = 1;
	      continue;

	    case 'e':
	      p_ad->exhaustive = 1;
	      continue;

	    case 'b':
	      if (++i >= argc)
		{
		  L("count expected");
		  exit(1);
		}
	      count = atoi(argv[i]);
	      continue;

	    case 'd':
	      if (++i >= argc)
		{
		  L("display mode expected");
		  exit(1);
		}
	      disp_mode = atoi(argv[i]);
	      continue;

	    case 'P':
	      if (++i >= argc)
		{
		  L("probability (in %%) expected");
		  exit(1);
		}
	      p_ad->prob_select_loc_min = atoi(argv[i]);
	      continue;

	    case 'f':
	      if (++i >= argc)
		{
		  L("freeze number expected");
		  exit(1);
		}
	      p_ad->freeze_loc_min = atoi(argv[i]);
	      continue;

	    case 'F':
	      if (++i >= argc)
		{
		  L("freeze number expected");
		  exit(1);
		}
	      p_ad->freeze_swap = atoi(argv[i]);
	      continue;

	    case 'l':
	      if (++i >= argc)
		{
		  L("reset limit expected");
		  exit(1);
		}
	      p_ad->reset_limit = atoi(argv[i]);
	      continue;

	    case 'p':
	      if (++i >= argc)
		{
		  L("reset percent expected");
		  exit(1);
		}
	      p_ad->reset_percent = atoi(argv[i]);
	      continue;

	    case 'a':
	      if (++i >= argc)
		{
		  L("restart limit expected");
		  exit(1);
		}
	      p_ad->restart_limit = atoi(argv[i]);
	      continue;

	    case 'r':
	      if (++i >= argc)
		{
		  L("restart number expected");
		  exit(1);
		}
	      p_ad->restart_max = atoi(argv[i]);
	      continue;

#ifdef CELL
	    case 't':
	      if (++i >= argc)
		{
		  L("number of threads expected");
		  exit(1);
		}
	      nb_threads = atoi(argv[i]);
	      continue;

	    case 'I':
	      read_initial = 2;
	      continue;
#endif
#if defined MPI
	    case 'C':
	      if (++i >= argc)
		{
		  L("#iterations before check for communication");
		  exit(1);
		}
	      count_to_communication = atoi(argv[i]);
	      continue;

            case 'I':
              read_initial = 3;
              continue;
#if (defined COMM_COST) || (defined ITER_COST) || (defined COMM_CONFIG)
	    case 'z':
	      if (++i >= argc)
		{
		  L("> #rand()*100 -> sends cost");
		  exit(1);
		}
	      proba_communication = atoi(argv[i]);
	      continue;
#endif
#endif /* MPI */
#ifdef PRINT_COSTS
	    case 'y':
	      if (++i >= argc)
		{
		  L("#filename pattern in which to save costs");
		  exit(1);
		}
	      filename_pattern_print_cost = argv[i];
	      continue;
#endif /* PRINT_COSTS */
	    case 'h':
	      fprintf(stderr, "Usage: %s [ OPTION ]", argv[0]);
	      if (param_needed > 0)
		fprintf(stderr, " PARAM");
	      else if (param_needed < 0)
		fprintf(stderr, " FILE");

	      L("");
	      L("   -i          read initial configuration");
	      L("   -D LEVEL    set debug mode (0=debug info, 1=step-by-step)");
	      L("   -L FILE     use file as log file");
	      L("   -c          check if the solution is valid");
	      L("   -s SEED     specify random seed");
	      L("   -b COUNT    bench COUNT times");
	      L("   -d WHAT     set display info (needs -b), WHAT is:");
              L("                 0=only last iter counters, 1=sum of restart+last iter counters (default)");
	      L("                 2=restart and last iter counters");
	      L("   -P PERCENT  probability to select a local min (instead of staying on a plateau)");
	      L("   -f NB       freeze variables of local min for NB swaps");
	      L("   -F NB       freeze variables swapped for NB swaps");
	      L("   -l LIMIT    reset some variables when LIMIT variable are frozen");
	      L("   -p PERCENT  reset PERCENT %% of variables");
	      L("   -a NB       abort and restart when NB iterations are reached");
	      L("   -r COUNT    restart at most COUNT times");
	      L("   -e          exhaustive seach (do all combinations)");
	      L("   -h          show this help");
#ifdef CELL
	      L("");
	      L("Cell specific options:");
	      L("   -t NB       launch NB threads");
	      L("   -I          set the same initial configuration to all threads");
#endif /* CELL */
#ifdef PRINT_COSTS
	      L("   -y pattern  pattern to create filename to save cost values");
#endif /* PRINT_COSTS */
#ifdef MPI
	      L("");
	      L("MPI specific options:");
	      L("   -C NB       check comm and send comm every NB iterations");
              L("   -I          set the same initial configuration to all processors");
#if (defined COMM_COST) || (defined ITER_COST)
	      L("   -z NB       rand()*100 < NB -> sends cost");
#endif /* (defined COMM_COST) || (defined ITER_COST) */
#endif /* MPI */
	      exit(0);

	    default:
	      fprintf(stderr, "unrecognized option %s (-h for a help)\n", argv[i]);
	      exit(1);
	    }
	}

      if (param_needed > 0 && !param_read)
	{
	  p_ad->param = atoi(argv[i]);
	  param_read = 1;
	}
      else if (param_needed < 0 && !param_read)
	{
	  strcpy(p_ad->param_file, argv[i]);
	  param_read = 1;
	}
      else
	{
	  fprintf(stderr, "unrecognized argument %s (-h for a help)\n", argv[i]);
	  exit(1);
	}
    }

  if (param_needed > 0 && !param_read)
    {
      printf("param: ");
      No_Gcc_Warn_Unused_Result(scanf("%d", &p_ad->param));
      getchar();		/* get the last \n */
    } 
  else if (param_needed < 0 && !param_read)
    {
      printf("file: ");
      No_Gcc_Warn_Unused_Result(fgets(p_ad->param_file, sizeof(p_ad->param_file), stdin));
      int l = strlen(p_ad->param_file);
      if (--l >= 0 && p_ad->param_file[l] == '\n')
	p_ad->param_file[l] = '\0';
    }
}

void
PrintAllCompilationOptions()
{ 
#if defined MPI
  PRINT0("Perform communications every %d iterations (default %d)\n",
	 count_to_communication, CBLOCK_DEFAULT ) ;
#if (defined COMM_COST)||(defined ITER_COST)||(defined COMM_CONFIG)
  PRINT0("Prob communication = %d\n", proba_communication) ;
#endif
#endif /* MPI */

  PRINT0("Compilation options:\n") ;
  PRINT0("- Backtrack when reset: ") ;
#if defined BACKTRACK
  PRINT0("ON\n") ;
#else
  PRINT0("OFF\n") ;
#endif

#if defined MPI
  PRINT0("- MPI (So forced count to 1 (-b 1)!\n") ;
#if defined DEBUG_MPI_ENDING
  PRINT0("- DEBUG_MPI_ENDING\n") ;
#endif
#if defined LOG_FILE
  PRINT0("- LOG_FILE\n") ;
#endif
#if defined NO_SCREEN_OUTPUT
  PRINT0("- NO_SCREEN_OUTPUT\n") ;
#endif
#if defined DISPLAY_0
  PRINT0("- DISPLAY_0\n") ;
#endif
#if defined DISPLAY_ALL
  PRINT0("- DISPLAY_ALL\n") ;
#endif
#if defined DEBUG
  PRINT0("- DEBUG\n") ;
#endif
#if defined DEBUG_QUEUE
  PRINT0("- DEBUG_QUEUE\n") ;
#endif
#if defined DEBUG_PRINT_QUEUE
  PRINT0("- DEBUG_PRINT_QUEUE\n") ;
#endif
#if defined MPI_ABORT
  PRINT0("- MPI_ABORT\n") ;
#endif
#if defined MPI_BEGIN_BARRIER
  PRINT0("- MPI_BEGIN_BARRIER\n") ;
#endif
  /* Heuristic for communications */
#if defined COMM_COST
  PRINT0("- With COMM_COST\n") ;
#elif defined ITER_COST
  PRINT0("- With ITER_COST\n") ;
#elif defined COMM_CONFIG
  PRINT0("- With COMM_CONFIG\n") ;
#else
  PRINT0("- Without comm exept for terminaison\n") ;
#endif
#endif /* MPI */

#if defined MPI_BEGIN_BARRIER
  PRINT0("===========================================\n\n") ;
  PRINT0("MPI Barrier called to synchronize processus before solve()\n");
  MPI_Barrier(MPI_COMM_WORLD);
#endif /* MPI_BEGIN_BARRIER */

}
