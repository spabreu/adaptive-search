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
#  define TPRINT0(...) do { printf("%ld: ",Real_Time()) ; printf(__VA_ARGS__); } while(0)
#endif

#if !defined(PRINTF)
#  define PRINTF(...)  do { printf(__VA_ARGS__); } while(0)
#endif

#if !defined(TPRINTF)
#  define TPRINTF(...) do { printf("%ld: ",Real_Time()) ; printf (__VA_ARGS__) ; } while(0)
#endif

#if !defined(TDPRINTF)
#  define TDPRINTF(...) do { DPRINTF("%ld: ",Real_Time()) ; DPRINTF(__VA_ARGS__) ; } while(0)
#endif

/*
 * For linear chaotic map functions
 */
# define DELTA_A	4
# define DELTA_C	4
# define OFFSET		16


#if defined BACKTRACK
#define SIZE_BACKTRACK 10
#define SIZE_VARVECTOR	5
#endif

/*-------*
 * Types *
 *-------*/

#if defined BACKTRACK
typedef struct backtrack_configuration
{
  unsigned int				*configuration;	/* Array of pb_size int storing configuration */
  int					varVector[SIZE_VARVECTOR+2];	/* Vector of VARIABLES to explore */
									/* varVector[0] = -1 */
									/* varVector[1] = number of var */
									/* varVector[k], with k > 1, = var index */

									/* OR */

									/* Vector of VALUES to explore */
									/* varVector[0] = x choosen */
									/* varVector[1] = number of values */
									/* varVector[k], with k > 1, = values index */

  int					cost;		/* cost of the configuration */
  int					resets;		/* number of performed resets */  
  int					local_mins;	/* number of local minimum found */
  int					swaps;		/* number of performed swaps */ 
  long long int				iterations;	/* number of performed iterations */
  struct backtrack_configuration	*previous;
  struct backtrack_configuration	*next;
} backtrack_configuration ;


typedef struct elitePool
{
  /* Use config array as circular array */
  /* backtrack_configuration	config_array[SIZE_BACKTRACK]; */
  /* int				config_array_begin; */
  /* int				config_array_end; */
  backtrack_configuration	*config_list_begin;
  backtrack_configuration	*config_list_end;
  int				config_list_size;
  int				nb_backtrack;		/* total number of performed backtrack (not real reset) */
  int				nb_variable_backtrack;	/* total number of performed backtrack where we start with another variable */
  int				nb_value_backtrack;	/* total number of performed backtrack where we start with another value */
} elitePool;

#endif /* BACKTRACK */

/*------------------*
 * Global variables *
 *------------------*/

#if defined BACKTRACK
<<<<<<< HEAD:src/tools.h
/* Use backtrack array as circular array */
backtrack_configuration Gbacktrack_array[SIZE_BACKTRACK] ;
int Gbacktrack_array_begin ;
int Gbacktrack_array_end ;
int Gconfiguration_size_in_bytes ;
=======
elitePool gl_elitePool;
elitePool gl_stockPool;
>>>>>>> 7b66fa6bbfadb0ac4e22536614e5a574b3486515:src/tools.h
#endif /* BACKTRACK */

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
/* To split a 64-bit long long int into two 32-bit unsigned int */
void split(long long int, unsigned int*, unsigned int*);

/* To concat two 32-bit unsigned int into a 64-bit long long int */
long long int concat(unsigned int, unsigned int);

/* To obtain a struct backtrack_configuration from the stock pool */
/* or from the elite pool if stock is empty */
backtrack_configuration* getFreeConfig();

/* Functions to push/pop into/from the elite pool */
void pushElite(backtrack_configuration*);
backtrack_configuration* popElite();

/* Functions to push/pop into/from the stock pool */
void pushStock(backtrack_configuration*);
backtrack_configuration* popStock();

/* void */
/* queue_configuration( backtrack_configuration * item ) */
/* backtrack_configuration * */
/* unqueue_configuration() */
#endif /* BACKTRACK */


#ifndef No_Gcc_Warn_Unused_Result
#define No_Gcc_Warn_Unused_Result(t) do { if(t) {} } while(0)
#endif

#endif /* _TOOLS_H */
