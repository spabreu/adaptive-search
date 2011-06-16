/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  tools.h: utilities header file
 */

#ifndef AS_TOOLS_H
#define AS_TOOLS_H

#include <limits.h>
#include <stdio.h>              /* printf() */
#include "tools_MPI.h"

#if defined PRINT_COSTS
#  include <unistd.h>           /* for read() */
#  include <errno.h>            /* for errno */
#endif /* PRINT_COSTS */

/*----------------------*
 * Constants and macros
 *----------------------*/

/* Macros to manage outputs of information.
   DPRINT -> Debug print: behavior defined at compilation
   TPRINT -> Time print.
*/
#if !defined(DPRINTF)
#  ifdef NO_SCREEN_OUTPUT
#    define DPRINTF(...) do { ((void) 0) ;} while(0)
#  elif defined(DEBUG) && (DEBUG & 4)
#    define DPRINTF(...) do { printf (__VA_ARGS__) ; } while(0)
#  elif defined DISPLAY_ALL
#    define DPRINTF(...) do { printf(__VA_ARGS__) ; } while(0)
#  elif !defined(DPRINTF)
#    define DPRINTF(...) do { ((void) 0) ;} while(0)
#  endif
#endif /* !defined(DPRINTF) */

#if !defined(PRINT0)
#  define PRINT0(...) do { printf(__VA_ARGS__); } while(0)
#endif
#if !defined(DPRINT0)
#  define DPRINT0(...) DPRINTF(__VA_ARGS__)
#endif
#if !defined(TPRINT0)
#  define TPRINT0(...) do { printf("%ld:",Real_Time()) ; printf(__VA_ARGS__); } while(0)
#endif

#if !defined(PRINTF)
#  define PRINTF(...)  do { printf(__VA_ARGS__); } while(0)
#endif

#if !defined(TPRINTF)
#  define TPRINTF(...) do { printf("%ld:",Real_Time()) ; printf (__VA_ARGS__) ; } while(0)
#endif

#if !defined(TDPRINTF)
#  define TDPRINTF(...) do { DPRINTF("%ld:",Real_Time()) ; DPRINTF(__VA_ARGS__) ; } while(0)
#endif

/*
 * For linear chaotic map functions
 */
# define DELTA_A	4
# define DELTA_C	4
# define OFFSET		16


#if defined BACKTRACK
#define SIZE_BACKTRACK 10
#endif

/*-------*
 * Types *
 *-------*/

#if defined BACKTRACK
typedef struct backtrack_configuration
{
  unsigned int * solution ; /* Array of pb_size int storing configuration */
  /* TODO: add a vector of permutations to be done,
           an index to know which have been done (oven value) to avoid memory
	     management of this array
	   and... ?
  */

} backtrack_configuration ;
#endif /* BACKTRACK */

/*------------------*
 * Global variables *
 *------------------*/

/* Use backtrack array as circular array */
backtrack_configuration Gbacktrack_array[SIZE_BACKTRACK] ;
int Gbacktrack_array_begin ;
int Gbacktrack_array_end ;
int Gconfiguration_size_in_bytes ;

/*------------*
 * Prototypes *
 *------------*/

/* Read from a file */
int
readn(int fd, char * buffer, int n) ;
/* Write to a file */
size_t
writen(int fd, const char * buffer, size_t n) ;

long Real_Time(void);

long User_Time(void);

void Randomize_Seed(unsigned seed);

unsigned Randomize(void);

unsigned Random(unsigned n);

int Random_Interval(int inf, int sup);

double Random_Double(void);

int		randChaos		(int max, int *var_last, int *var_a, int *var_c);
void		randChaosInit		(void);
double		randChaosDouble		(void);
unsigned int	randChaosInt		(unsigned int n);

void Random_Array_Permut(int *vec, int size);

void Random_Permut(int *vec, int size, const int *actual_value, int base_value);

void Random_Permut_Repair(int *vec, int size, const int *actual_value, int base_value);

int Random_Permut_Check(int *vec, int size, const int *actual_value, int base_value);

#if defined BACKTRACK
/* Functions to manage the configurations array in a circular way */
void
queue_configuration( backtrack_configuration * item ) ;
backtrack_configuration * 
unqueue_configuration() ;
#endif /* BACKTRACK */


#ifndef No_Gcc_Warn_Unused_Result
#define No_Gcc_Warn_Unused_Result(t) do { if(t) {} } while(0)
#endif

#endif /* _TOOLS_H */
