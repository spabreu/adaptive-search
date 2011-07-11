/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  main.h: MPI extension for main_MPI.h
 */

#if !defined(AS_MAIN_MPI_H) && defined(MPI)
#define AS_MAIN_MPI_H

#include <mpi.h>
/*#include <unistd.h>*/                   /* sleep() */
#include <math.h>
#include <time.h>                         /* time() */
#include <limits.h>                       /* To initiate first random seed */

//#include "ad_solver.h"                    /* For macros and global vars */

/*-----------*
 * Constants *
 *-----------*/

#define RESULTS_CHAR_MSG_SIZE 256 /* with \n */

/*-------*
 * Types *
 *-------*/

typedef struct Main_MPIData Main_MPIData ;
typedef struct AdData AdData ;

struct Main_MPIData
{
  char results[RESULTS_CHAR_MSG_SIZE] ;       /* To communicate performances */

  int * param_a_ptr ;
  int * param_c_ptr ;
  int * last_value_ptr ;
  
  AdData * p_ad ;
  
  long int * print_seed_ptr ;  /* value to print to redo this scenario */

  time_t tv_sec ;

  int * count_ptr ;

  int size_message ; /* message = (range + config + stats + etc) */ 

#if defined COMM_CONFIG
  /* To store messages, reserve a big block of memory here */
  unsigned int * block_of_messages ;
#endif

#if defined PRINT_COSTS
  int * nb_digits_nbprocs_ptr ;
#endif

} ;

/*------------------*
 * Global variables *
 *------------------*/

/*------------*
 * Prototypes *
 *------------*/

void
AS_MPI_initialization( Main_MPIData * mpi_data_ptr ) ;

/* Because we need to initialize some memory whose amount is known
   only once p_ad si fully populated, thus at the end of initialization */
void
AS_MPI_initialization_epilogue( Main_MPIData * mpi_data_ptr ) ;

void
AS_MPI_completion( Main_MPIData * mpi_data_ptr ) ;

#endif /* !defined(AS_MAIN_MPI_H) && defined(MPI) */
