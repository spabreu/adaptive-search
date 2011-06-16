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
#if defined DEBUG_QUEUE
#  include <assert.h>
#endif

/*----------------------*
 * Constants and macros
 *----------------------*/

#if defined DEBUG_MPI_ENDING && !defined DISPLAY_0
#define DISPLAY_ALL 1
#endif

#if (defined DEBUG_PRINT_QUEUE)&&!(defined DEBUG_QUEUE)
#  define DEBUG_QUEUE
#endif

#if defined ITER_COST
#  define SIZE_MESSAGE 3 /* Use 3 for nb iter */
#else
#  define SIZE_MESSAGE 2
#endif

#define QUEUE_NAME_MAX_LENGTH 20

/* Macros to manage outputs of information.
   DPRINT  -> Debug print: behavior defined at compilation
   TPRINT  -> Time print.
   TDPRINT -> Time Debug print.

   For MPI programs, 
   choose if all proc make output (race on screen/network)
   or only proc 0
   or no output (batch, better tp print output in a file
*/
#if defined DISPLAY_0
#  define DPRINT0(...) do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)
#  define PRINTF(...)  do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)
#  define DPRINTF(...) do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)
#  define TPRINTF(...) do { if( my_num == 0 ) { printf("%ld:",Real_Time()) ; printf (__VA_ARGS__) ; } } while(0)
#endif

#if defined(DEBUG) && !defined(DISPLAY_0)
#  define DPRINT0(...) do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)
#else
#  define DPRINT0(...) do { ((void) 0) ;} while(0)
#endif

#define PRINT0(...)  do { if( my_num == 0 ) printf(__VA_ARGS__); } while(0)
#define TPRINT0(...) do { if( my_num == 0 ) { printf("%ld:",Real_Time()) ; printf(__VA_ARGS__); } } while(0)

#if !defined(DISPLAY_0)
#  define PRINTF(...)  do { printf(__VA_ARGS__); } while(0)
#  define TPRINTF(...) do { printf("%ld:",Real_Time()) ; printf(__VA_ARGS__); } while(0)
#endif

/*-------*
 * Types *
 *-------*/

typedef struct tegami
{
#if defined DEBUG_QUEUE
  char * text ;
  int size ;
  int nb_max_msgs_used ;
#endif
#if defined COMM_CONFIG
  configuration configuration ;
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
