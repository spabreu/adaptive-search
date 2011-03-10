/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  tools_MPI.h: MPI-specific utilities header file
 */

#if !defined(AS_TOOLS_MPI_H) && defined(MPI)

#define AS_TOOLS_MPI_H
#include <stdio.h>       /* printf in DPRINTF macro */
#include <mpi.h>
#if defined PRINT_COSTS
#  include <unistd.h>      /* for read() */
#  include <errno.h>       /* for errno */
#endif /* PRINT_COSTS */
#if defined YC_DEBUG_QUEUE
#  include <assert.h>
#endif


/*----------------------*
 * Constants and macros
 *----------------------*/

/* For MPI programs, 
   choose if all proc make output (race on screen/network)
   or only proc 0
   or no output (batch, better tp print output in a file
*/
#if defined DISPLAY_0
#  define DPRINTF(...) do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)
#endif
#define PRINT0(...) do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)

#if (defined YC_DEBUG_PRINT_QUEUE)&&!(defined YC_DEBUG_QUEUE)
#  define YC_DEBUG_QUEUE
#endif


#if defined ITER_COST
#  define SIZE_MESSAGE 3 /* Use 3 for nb iter */
#else
#  define SIZE_MESSAGE 2
#endif

/*-------*
 * Types *
 *-------*/

typedef struct tegami
{
#if defined YC_DEBUG_QUEUE
  char * text ;
  int size ;
  int nb_max_msgs_used ;
#endif
  unsigned int message[SIZE_MESSAGE] ;
  MPI_Status status ;                /* Status of communication */
  MPI_Request handle ;               /* Handle on communication */
  struct tegami * next ;
  struct tegami * previous ;
} tegami ;

/*------------------*
 * Global variables *
 *------------------*/

/*------------*
 * Prototypes *
 *------------*/

/* Read from a file */
int
readn(int fd, char * buffer, int n) ;
/* Write to a file */
size_t
writen(int fd, const char * buffer, size_t n) ;
/* Insert the item at head of list */
void
push_tegami_on( tegami * msg, tegami * list) ;
/* Drop and return the head of the list */
tegami *
get_tegami_from( tegami * list) ;
/* Return the last elt */
tegami * 
unqueue_tegami_from( tegami * list) ;

#endif /* !defined(AS_TOOLS_MPI_H) && defined(MPI) */
