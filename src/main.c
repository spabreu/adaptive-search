/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  main.c: benchmark main function
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

/*------------------*
 * Global variables *
 *------------------*/

int nb_threads;			/* must be ld-global! */

static int count;
static int disp_mode;
static int check_valid;
static int read_initial;	/* 0=no, 1=yes, 2=all threads use the same (CELL specific) */


int param_needed;		/* overwritten by benches if an argument is needed */


/*------------*
 * Prototypes *
 *------------*/

static void Set_Initial(AdData *p_ad);

static void Verify_Sol(AdData *p_ad);

static void Parse_Cmd_Line(int argc, char *argv[], AdData *p_ad);

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
  int i;

  double time_one0, time_one;
  double nb_same_var_by_iter, nb_same_var_by_iter_tot;

  int nb_iter_cum;
  int nb_local_min_cum;
  int nb_swap_cum;
  int nb_reset_cum;
  double nb_same_var_by_iter_cum;

  int nb_restart_cum,                nb_restart_min,              nb_restart_max;
  double time_cum,                   time_min,                    time_max;

  int nb_iter_tot_cum,               nb_iter_tot_min,             nb_iter_tot_max;
  int nb_local_min_tot_cum,          nb_local_min_tot_min,        nb_local_min_tot_max;
  int nb_swap_tot_cum,               nb_swap_tot_min,             nb_swap_tot_max;
  int nb_reset_tot_cum,              nb_reset_tot_min,            nb_reset_tot_max;
  double nb_same_var_by_iter_tot_cum, nb_same_var_by_iter_tot_min, nb_same_var_by_iter_tot_max;


  char buff[256];

  Parse_Cmd_Line(argc, argv, p_ad);

  if (p_ad->seed < 0)
    p_ad->seed = Randomize();
  else
    Randomize_Seed(p_ad->seed);

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
    printf("Warning ad_solver is not compiled with debugging support\n");

  if (p_ad->log_file && !ad_has_log_file)
    printf("Warning ad_solver is not compiled with log file support\n");

  p_ad->size_in_bytes = p_ad->size * sizeof(int);
  p_ad->sol = malloc(p_ad->size_in_bytes);

  if (p_ad->nb_var_to_reset == -1)
    p_ad->nb_var_to_reset = Div_Round_Up(p_ad->size * p_ad->reset_percent, 100);

  printf("current random seed used: %d\n", p_ad->seed);
  printf("variables of loc min are frozen for: %d swaps\n", p_ad->freeze_loc_min);
  printf("variables swapped    are frozen for: %d swaps\n", p_ad->freeze_swap);
  if (p_ad->reset_percent >= 0)
    printf("%d %% = ", p_ad->reset_percent);
  printf("%d variables are reset when %d variables are frozen\n", 
	 p_ad->nb_var_to_reset, p_ad->reset_limit);
  printf("probability to select a local min (instead of staying on a plateau): ");
  if (p_ad->prob_select_loc_min >=0 && p_ad->prob_select_loc_min <= 100)
    printf("%d %%\n", p_ad->prob_select_loc_min);
  else
    printf("not used\n");
  printf("abort when %d iterations are reached "
	 "and restart at most %d times\n",
	 p_ad->restart_limit, p_ad->restart_max);

  if (count <= 0)
    {
      Set_Initial(p_ad);

      p_ad->seed = Random(65536);
      time_one0 = (double) User_Time();
      Solve(p_ad);
      time_one = ((double) User_Time() - time_one0) / 1000;

      if (p_ad->exhaustive)
	printf("exhaustive search\n");

      if (count < 0)
	Display_Solution(p_ad);

      Verify_Sol(p_ad);

      if (p_ad->total_cost)
	printf("*** NOT SOLVED (cost of this pseudo-solution: %d) ***\n", p_ad->total_cost);

      if (count == 0)
	{
	  nb_same_var_by_iter = (double) p_ad->nb_same_var / p_ad->nb_iter;
	  nb_same_var_by_iter_tot = (double) p_ad->nb_same_var_tot / p_ad->nb_iter_tot;

	  printf("%5d %7.2f %7d %7d %7d %7d %7.1f %7d %7d %7d %7d %7.1f\n", 
		 p_ad->nb_restart, time_one, 
		 p_ad->nb_iter, p_ad->nb_local_min, p_ad->nb_swap, 
		 p_ad->nb_reset, nb_same_var_by_iter,
		 p_ad->nb_iter_tot, p_ad->nb_local_min_tot, p_ad->nb_swap_tot, 
		 p_ad->nb_reset_tot, nb_same_var_by_iter_tot);
	}
      else
	{
	  printf("in %.2f secs (%d iters, %d swaps, %d restarts)\n", 
		 time_one, p_ad->nb_iter_tot, p_ad->nb_swap_tot, p_ad->nb_restart);
	}

      return 0;
    }

  putchar('\n');

  sprintf(buff, "|Count|restart|    time |   iters | loc min |   swaps "
	  "|  resets |same/iter|\n");

  if (param_needed)
    printf("%*d\n", (int) strlen(buff)/2, p_ad->param);
  
  printf("%s", buff);
  for(i = 0; buff[i] != '\n'; i++)
    if (buff[i] != '|')
      buff[i] = '-';
  printf("%s", buff);
  printf("\n\n");

  
  nb_restart_cum = time_cum = 0;

  nb_iter_cum = nb_local_min_cum = nb_swap_cum = nb_reset_cum = 0;
  nb_same_var_by_iter_cum = 0;


  nb_iter_tot_cum = nb_local_min_tot_cum = nb_swap_tot_cum = nb_reset_tot_cum = 0;
  nb_same_var_by_iter_tot_cum = 0;

  nb_restart_min = (1 << 30);
  time_min = 1e100;
  
  nb_iter_tot_min = nb_local_min_tot_min = nb_swap_tot_min = nb_reset_tot_min = (1 << 30);
  nb_same_var_by_iter_tot_min = 1e100;

  nb_restart_max = 0;
  time_max = 0;
 
  nb_iter_tot_max = nb_local_min_tot_max = nb_swap_tot_max = nb_reset_tot_max = 0;
  nb_same_var_by_iter_tot_max = 0;


  for(i = 1; i <= count; i++)
    {
      Set_Initial(p_ad);

      p_ad->seed = Random(65536);
      time_one0 = (double) User_Time();
      Solve(p_ad);
      time_one = ((double) User_Time() - time_one0) / 1000;

      if (disp_mode == 2 && nb_restart_cum > 0)
	printf("\033[A\033[K");
      printf("\033[A\033[K\033[A\033[256D");


      Verify_Sol(p_ad);

      nb_same_var_by_iter = (double) p_ad->nb_same_var / p_ad->nb_iter;
      nb_same_var_by_iter_tot = (double) p_ad->nb_same_var_tot / p_ad->nb_iter_tot;

      nb_restart_cum += p_ad->nb_restart;
      time_cum += time_one;
      nb_iter_cum += p_ad->nb_iter;
      nb_local_min_cum += p_ad->nb_local_min;
      nb_swap_cum += p_ad->nb_swap;
      nb_reset_cum += p_ad->nb_reset;
      nb_same_var_by_iter_cum += nb_same_var_by_iter;

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


      switch(disp_mode)
	{
	case 0:			/* only last iter counters */
	case 2:			/* last iter followed by restart if needed */
	  printf("|%4d | %5d%c| %7.2f | %7d | %7d | %7d | %7d | %7.1f |\n",
		 i, p_ad->nb_restart, (p_ad->total_cost == 0) ? ' ' : 'N', time_one,
		 p_ad->nb_iter, p_ad->nb_local_min, p_ad->nb_swap,
		 p_ad->nb_reset, nb_same_var_by_iter);

	  if (disp_mode == 2 && p_ad->nb_restart > 0)
	    printf("|     |       |         | %7d | %7d | %7d | %7d | %7.1f |\n",
		   p_ad->nb_iter_tot, p_ad->nb_local_min_tot, p_ad->nb_swap_tot,
		   p_ad->nb_reset_tot, nb_same_var_by_iter_tot);

	  printf("%s", buff);

	  printf("| avg | %5d | %7.2f | %7d | %7d | %7d | %7d | %7.1f |\n",
		 nb_restart_cum / i, time_cum / i,
		 nb_iter_cum / i, nb_local_min_cum / i, nb_swap_cum / i,
		 nb_reset_cum / i, nb_same_var_by_iter_cum / i);

	  if (disp_mode == 2 && nb_restart_cum > 0)
	    printf("|     |       |         | %7d | %7d | %7d | %7d | %7.1f |\n",
		   nb_iter_tot_cum / i, nb_local_min_tot_cum / i, nb_swap_tot_cum / i,
		   nb_reset_tot_cum / i, nb_same_var_by_iter_tot_cum / i);

	  break;

	case 1:			/* only total (restart + last iter) counters */
	  printf("|%4d | %5d%c| %7.2f | %7d | %7d | %7d | %7d | %7.1f |\n",
		 i, p_ad->nb_restart, (p_ad->total_cost == 0) ? ' ' : 'N', time_one,
		 p_ad->nb_iter_tot, p_ad->nb_local_min_tot, p_ad->nb_swap_tot,
		 p_ad->nb_reset_tot, nb_same_var_by_iter_tot);

	  printf("%s", buff);

	  printf("| avg | %5d | %7.2f | %7d | %7d | %7d | %7d | %7.1f |\n",
		 nb_restart_cum / i, time_cum / i,
		 nb_iter_tot_cum / i, nb_local_min_tot_cum / i, nb_swap_tot_cum / i,
		 nb_reset_tot_cum / i, nb_same_var_by_iter_tot_cum / i);
	  break;
	}
    }

  if (count <= 0)
    return 0;

  printf("| min | %5d | %7.2f | %7d | %7d | %7d | %7d | %7.1f |\n",
	 nb_restart_min, time_min,
	 nb_iter_tot_min, nb_local_min_tot_min, nb_swap_tot_min,
	 nb_reset_tot_min, nb_same_var_by_iter_tot_min);

  printf("| max | %5d | %7.2f | %7d | %7d | %7d | %7d | %7.1f |\n",
	 nb_restart_max, time_max,
	 nb_iter_tot_max, nb_local_min_tot_max, nb_swap_tot_max,
	 nb_reset_tot_max, nb_same_var_by_iter_tot_max);



  return 0;
}



void
Set_Initial(AdData *p_ad)
{
  int i;
  switch (read_initial)
    {
    case 0:
      break;

    case 1:
      printf("enter the initial solution:\n");
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


  for(i = 1; i < argc; i++)
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

	    case 'h':
	      fprintf(stderr, "Usage: %s [ OPTION ]", argv[0]);
	      if (param_needed)
		fprintf(stderr, " PARAM");

	      L("");
	      L("   -i          read initial position");
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
#endif
	      exit(0);

	    default:
	      fprintf(stderr, "unrecognized option %s (-h for a help)\n", argv[i]);
	      exit(1);
	    }
	}
      if (param_needed)
	p_ad->param = atoi(argv[i]);
      else
	{
	  fprintf(stderr, "unrecognized argument %s (-h for a help)\n", argv[i]);
	  exit(1);
	}
    }

  if (param_needed && p_ad->param < 0)
    {
      printf("param :");
      if (scanf("%d", &p_ad->param))	/* avoid gcc warning warn_unused_result */
	{}
      getchar();		/* get the last \n */
    }
}

