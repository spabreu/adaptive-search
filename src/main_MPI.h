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

#include "ad_solver.h"                    /* For macros and global vars */

/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/

typedef struct
{
  char results[RESULTS_CHAR_MSG_SIZE] ;       /* To communicate performances */

  int * param_a_ptr ;
  int * param_c_ptr ;
  int * last_value_ptr ;
  
  AdData * p_ad ;
  
  long int * print_seed_ptr ;

  time_t tv_sec ;

  int * count_ptr ;

#if defined PRINT_COSTS
  int * nb_digits_nbprocs_ptr ;
#endif

} Main_MPIData ;

/*------------------*
 * Global variables *
 *------------------*/

/*------------*
 * Prototypes *
 *------------*/

void
AS_MPI_initialization( Main_MPIData * mpi_data_ptr ) ;

void
AS_MPI_completion( Main_MPIData * mpi_data_ptr ) ;

#endif /* !defined(AS_MAIN_MPI_H) && defined(MPI) */
