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
#define CBLOCK_DEFAULT 2000

/*-------*
 * Types *
 *-------*/

#define NBMAXSTEPS 16               /* log2 nbprocs */
typedef enum {                      /* Add a protocol => update ad_solver.c! */
  NOT_A_PROTOCOL,
  LS_KILLALL,                       /* msg = [range ; proc finished] */
  SENDING_RESULTS,
  LS_COST,                          /* msg = [range ; cost] */
  SEED,
  LS_NBMSGS
} protocol_msg ;                    /* All kind of messages between processus */

typedef struct
{
  /* Number of C_iter */
  unsigned int nb_block ;
#if (defined COMM_COST) || (defined ITER_COST)
  /* Store the min of the recv cost */
  unsigned int mixed_received_msg_cost ;
  /* The last cost that we sent */
  unsigned int best_cost_sent ;
  /* The last cost that we received */
  unsigned int best_cost_received ;
  /* Message for communication in the following shape:       */
  /* - COMM_COST (range ; cost)                              */
  /* - ITER_COST (range ; cost ; iter)                       */
  unsigned int s_cost_message[SIZE_MESSAGE] ;
#endif /* COMM_COST || ITER_COST */
#if defined ITER_COST
  /* #iter of best cost sent */
  int iter_of_best_cost_sent ;
  int mixed_received_msg_iter ;
  /* unused for the moment */
  int iter_for_best_cost_received ;
#endif /* ITER_COST */
#if defined MIN_ITER_BEFORE_RESTART
  unsigned int nbiter_since_restart ;
#endif

#if defined COMM_CONFIG
  configuration list_of_configurations ; /* To store points of backtrack */
#endif

} Ad_Solve_MPIData ;

/*------------------*
 * Global variables *
 *------------------*/

int mpi_size ;
unsigned int count_to_communication ; /* nb of iter before checking 
					 receive of msg */
char * protocole_name [LS_NBMSGS] ; /* Instanciated in ad_solver.c! */
tegami * the_message ;              /* Used as temporary variable */
tegami list_sent_msgs ;
tegami list_allocated_msgs ;        /* Careful: msgs not initialized! */
tegami list_recv_msgs ;

#if (defined COMM_COST) || (defined ITER_COST)
unsigned int proba_communication ;  /* > (rand*100) -> sends cost */
#endif

/*------------*
 * Prototypes *
 *------------*/

/* Initialize structure fields */
void
Ad_Solve_init_MPI_data( Ad_Solve_MPIData * mpi_data ) ;
/* If returns 10, we have to goto for a restart, a reset,... act! */
int
Ad_Solve_manage_MPI_communications( Ad_Solve_MPIData * mpi_data,
				    AdData * ad_data ) ;
/* Goto (and don't return!) the normal ending of ad_solver without MPI */
void
free_data_of_ad_solver() ;
/* Free pending msg, structures, etc. with global var */
void
dead_end_final() ;
/* range | proc finished */
void
send_log_n( unsigned int[], protocol_msg ) ;

#endif /* !defined(AS_AD_SOLVER_MPI_H) && defined(MPI) */
