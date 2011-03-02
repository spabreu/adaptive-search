/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  tools.h: utilities header file
 */

#ifndef AS_TOOLS_H
#define AS_TOOLS_H

#if defined MPI
#include <stdio.h>       /* printf in DPRINTF macro */
#include <mpi.h>
#if defined PRINT_COSTS
#include <unistd.h>      /* for read() */
#include <errno.h>       /* for errno */
#endif /* PRINT_COSTS */
#endif /* MPI */

/*-----------*
 * Constants *
 *-----------*/

/* For MPI programs, 
   choose if all proc make output (race on screen/network)
   or only proc 0
   or no output (batch, better tp print output in a file
   
   For normal use, and cell...
*/
#ifdef NO_SCREEN_OUTPUT
#define DPRINTF(...) { ((void) 0) ;}
#elif defined DISPLAY_0
#define DPRINTF(...) { if( my_num == 0 ) printf(__VA_ARGS__); }
#elif defined DISPLAY_ALL
#define DPRINTF(...) { printf(__VA_ARGS__) ; }
#elif defined(DEBUG) && (DEBUG & 4)
#define DPRINTF(...) { printf (__VA_ARGS__) ; }
#else
#define DPRINTF(...) { ((void) 0) ; }
#endif

#if !(defined MPI)
#define PRINT0(...) DPRINTF(...)
#else
#define PRINT0(...) { if( my_num == 0 ) printf(__VA_ARGS__); }
#endif


#if defined MPI

#if (defined YC_DEBUG_PRINT_QUEUE)&&!(defined YC_DEBUG_QUEUE)
#define YC_DEBUG_QUEUE
#endif

#if defined YC_DEBUG_QUEUE
#include <assert.h>
#endif

#if defined ITER_COST
#define SIZE_MESSAGE 3 /* Use 3 for nb iter */
#else
#define SIZE_MESSAGE 2
#endif

#endif /* MPI */

/*-------*
 * Types *
 *-------*/

#if defined MPI
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

#endif /* MPI */

/*------------------*
 * Global variables *
 *------------------*/

/*------------*
 * Prototypes *
 *------------*/

long Real_Time(void);

long User_Time(void);

void Randomize_Seed(unsigned seed);

unsigned Randomize(void);

unsigned Random(unsigned n);

int Random_Interval(int inf, int sup);

double Random_Double(void);


void Random_Permut(int *vec, int size, const int *actual_value, int base_value);

void Random_Permut_Repair(int *vec, int size, const int *actual_value, int base_value);

int Random_Permut_Check(int *vec, int size, const int *actual_value, int base_value);


#ifndef No_Gcc_Warn_Unused_Result
#define No_Gcc_Warn_Unused_Result(t) do { if(t) {} } while(0)
#endif

#if defined MPI
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
#endif /* MPI */


#endif /* _TOOLS_H */
