/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  tools.h: utilities header file
 */

#ifndef AS_TOOLS_H
#define AS_TOOLS_H

#include <stdio.h>              /* printf() */
#include "tools_MPI.h"

/*----------------------*
 * Constants and macros
 *----------------------*/

/* Macros to manage outputs of information.
*/
#ifdef NO_SCREEN_OUTPUT
#  define DPRINTF(...) do { ((void) 0) ;} while(0)
#elif defined(DEBUG) && (DEBUG & 4)
#  define DPRINTF(...) do { printf (__VA_ARGS__) ; } while(0)
#elif defined DISPLAY_ALL
#  define DPRINTF(...) do { printf(__VA_ARGS__) ; } while(0)
#elif !defined(DPRINTF)
#  define DPRINTF(...) do { ((void) 0) ; } while(0)
#endif

#if !defined(PRINT0)
#define PRINT0(...) DPRINTF(...)
#endif

/*-------*
 * Types *
 *-------*/

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

#endif /* _TOOLS_H */
