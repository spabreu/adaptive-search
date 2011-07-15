/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  ad_solver_MPI.h: MPI extension of ad_solver
 */

#if !defined(AS_AD_SOLVER_MPI_H) && defined(MPI)
#define AS_AD_SOLVER_MPI_H

#include "ad_solver.h" /* AdData structure */
#include "tools_MPI.h"
#include <mpi.h>
#include <limits.h> /* INT_MAX */
#include <assert.h> /* For assert(). gcc -D NDEBUG to get ride of tests */
#if defined DEBUG
#  if !defined( DEBUG_MPI )
#    define DEBUG_MPI
#  endif
#  define DEBUG_RESTART
#endif /* DEBUG */

/*----------------------*
 * Constants and macros
 *----------------------*/

/* #define AD_SOLVE_MANAGE_MPI_COMM(...)  */
#define CBLOCK_DEFAULT 1000

/*-------*
 * Types *
 *-------*/

#define NBMAXSTEPS 16        /* log2 nbprocs */
typedef enum {               /* Add a protocol => update ad_solver_MPI.c! */
  NOT_A_PROTOCOL,
  LS_KILLALL,                /* msg = [range ; proc finished] */
  SENDING_RESULTS,
  LS_COST,                   /* msg = [range ; cost] */
  LS_ITER,                   /* msg = [range ; iter] */
  LS_COST_ITER,                   /* msg = [range ; iter] */
  LS_CONFIG,                 /* msg = [range ; config ; cost ; iter] */
  LS_NBMSGS                  /* Number of protocols! */
} protocol_msg ;             /* Defines protocol of messages between procs */

typedef struct
{
  /* To access some information on pb or MPI data (like size_message) */
  AdData * p_ad ;

  /* Number of C_iter */
  unsigned int nb_block ;

#if defined COMM_COST
  /* Store the min of the recv cost per C-block */
  unsigned int min_cost_received ;
  /* The last cost that we sent */
  unsigned int best_cost_sent ;
  /* The best cost ever received */
  unsigned int total_min_cost_received ;
  /* Message for communication in the following shape:       */
  /* - COMM_COST (range ; cost)                              */
  unsigned int * s_cost_message ;
#endif

#if defined ITER_COST
  /* Store the min of the recv cost per C-block */
  unsigned int min_cost_received ;
  /* The last cost that we sent */
  unsigned int best_cost_sent ;
  /* The best cost ever received */
  unsigned int total_min_cost_received ;
  /* Message for communication in the following shape:       */
  /* - ITER_COST (range ; cost ; iter)                       */
  unsigned int * s_cost_message ;

  /* #iter of best cost sent */
  int iter_of_best_cost_sent ;
  /* Store the min of the recv iter per C-block */
  int min_received_msg_iter ;
  int iter_for_best_cost_received ;
#endif

#if defined MIN_ITER_BEFORE_RESTART
  unsigned int nbiter_since_restart ;
#endif

#if defined COMM_CONFIG
  /* Store the min of the recv cost per C-block */
  unsigned int min_cost_received ;
  /* The last cost that we sent */
  unsigned int best_cost_sent ;
  /* The best cost ever received */
  unsigned int total_min_cost_received ;
  /* Message for communication in the following shape:       */
  /* - COMM_CONFIG (range ; config ; cost ; iter)            */
  unsigned int * s_cost_message ;

  /* #iter of best cost sent */
  int iter_of_best_cost_sent ;
  /* Store the min of the recv iter per C-block */
  int min_received_msg_iter ;

  int iter_for_best_cost_received ;

  /* cpy of best msg rcvd on a C-block */
  unsigned int * cpy_best_msg ; /* memory allocated at beginning */
  /* temporary ptr on best msg */
  unsigned int * tmp_best_msg_ptr ; /* only a copy of ptr! */
#endif
} Ad_Solve_MPIData ;

/*------------------*
 * Global variables *
 *------------------*/

#if defined STATS
typedef struct Ad_Solve_MPIStats {
  unsigned int nb_sent_messages ;
  unsigned int nb_sent_mymessages ;
} Ad_Solve_MPIStats ;

Ad_Solve_MPIStats Gmpi_stats ;
#endif

int mpi_size ;
unsigned int count_to_communication ; /* nb of iter before checking 
					 receive of msg */
char * protocole_name [LS_NBMSGS] ; /* Instanciated in ad_solver.c! */
tegami * Gthe_message ;
tegami list_sent_msgs ;
tegami list_allocated_msgs ;        /* Careful: msgs not initialized! */
tegami list_recv_msgs ;

#if (defined COMM_COST) || (defined ITER_COST) || (defined COMM_CONFIG)
unsigned int proba_communication ;  /* > (rand*100) -> sends cost */
unsigned int cost_threshold ;
#endif /* (defined COMM_COST) || (defined ITER_COST) || (defined COMM_CONFIG) */

/*------------*
 * Prototypes *
 *------------*/

/* Initialize structure fields */
void
Ad_Solve_init_MPI_data( Ad_Solve_MPIData * mpi_data ) ;
/* If returns 10, we have to goto for a restart, a reset,... act! */
int
Ad_Solve_manage_MPI_communications( Ad_Solve_MPIData * mpi_data ) ;
/* Goto (and don't return!) the normal ending of ad_solver without MPI */
void
free_data_of_ad_solver() ;
/* Free pending msg, structures, etc. with global var */
void
dead_end_final() ;
/* Try to free old messages in their sending order */
void
Ad_Solver_free_messages( Ad_Solve_MPIData * mpi_data_ptr ) ;
/* Sends on a logn tree */
void
send_log_n( unsigned int size_message, unsigned int * msg, protocol_msg ) ;

#endif /* !defined(AS_AD_SOLVER_MPI_H) && defined(MPI) */
