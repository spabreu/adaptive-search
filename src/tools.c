/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  tools.c: utilities
 */

#if defined(CELL) && !defined(AS_SPU)
# define HAVE_FTIME
#else
# undef HAVE_FTIME
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_FTIME
#include <sys/timeb.h>
#endif

#if defined(__unix__) || defined(__CYGWIN__)
#include <sys/time.h>
#include <sys/resource.h>
#endif


#include "tools.h"

/*-----------*
 * Constants *
 *-----------*/

/*-------*
 * Types *
 *-------*/

/*------------------*
 * Global variables *
 *------------------*/

static long start_real_time = 0;


/*------------*
 * Prototypes *
 *------------*/

/*
 *  USER_TIME
 *
 *  returns the user time in msecs.
 */
long
User_Time(void)
{
  long user_time;

#if defined(__unix__)
  struct rusage rsr_usage;

  getrusage(RUSAGE_SELF, &rsr_usage);

  user_time = (rsr_usage.ru_utime.tv_sec * 1000) +
    (rsr_usage.ru_utime.tv_usec / 1000);

#elif defined(M_ix86_cygwin)	/* does not work well, returns real_time */

  struct tms time_buf1;

  times(&time_buf1);
  user_time = time_buf1.tms_utime * 1000 / tps;

#elif defined(M_ix86_win32)

  user_time = (long) ((double) clock() * 1000 / CLOCKS_PER_SEC);

#else

  user_time = 0;
#warning user_time not available

#endif

  return user_time;
}



/*
 *  REAL_TIME
 *
 *  returns the real time in msecs.
 */
long
Real_Time(void)
{
  long real_time;

#if defined(__unix__) && !defined(__CYGWIN__)
  struct timeval tv;

  gettimeofday(&tv, NULL);
  real_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

#elif defined(_WIN32) || defined(__CYGWIN__)

  real_time = (long) ((double) clock() * 1000 / CLOCKS_PER_SEC);

#else

  real_time = 0;
#warning real_time not available

#endif

  return real_time - start_real_time;
}




/*
 *  RANDOMIZE_SEED
 *
 *  Initializes the random number generator with a given seed.
 *  Returns the seed.
 */
unsigned
Randomize_Seed(unsigned seed)
{
  srand(seed);
  return seed;
}



/*
 *  RANDOMIZE
 *
 *  Initializes the random number generator with a random seed.
 *  Returns the seed.
 */
unsigned
Randomize(void)
{
  unsigned seed;
#ifdef HAVE_FTIME
  struct timeb tp;
  ftime(&tp);
  seed = (unsigned) tp.time * 1000 + (unsigned) tp.millitm;
#else
  seed = (unsigned) time(NULL);
#endif

  Randomize_Seed(seed);
  seed = Random(65536);
  Randomize_Seed(seed);
  return seed;
}



/*
 *  RANDOM
 *
 *  Returns a random number in 0..n-1.
 */
unsigned
Random(unsigned n)
{
#if 1
  return (unsigned) ((double) n * rand() / (RAND_MAX + 1.0));
#else
  unsigned res = (unsigned) ((double) n * rand() / (RAND_MAX + 1.0));
  printf("random(%d) = %d\n", n, res);
  return res;
#endif
}



/*
 *  RANDOM_PERMUT
 *
 *  Generate a vector of size elements with a random permutation 
 *  - of values in base_value..base_value+size-1 (if actual_value == NULL)
 *  - of values in actual_value[] + base_value
 *
 *  To have no problem with negative numbers (base_value < 0 and/or
 *  negative values un actual_value[]) the generation of the permutation
 *  is done on 0..size-1
 *  
 *  To ensure the permutation we mark each random generated integer
 *  (in 0..size-1) in vec setting the bit sign.
 *
 *  When a random number is generated we check if it is marked, if yes
 *  another random number is tried. It is possible to bound the search
 *  setting the macro PERMUT_MAX_TRIES to the max number of tries
 *  for each index (e.g. a constant 1000 or 'size' or 'size * size')
 *  Experiments shows that it is better to not set a bound.
 *
 *  At the end, marks are reset and the permutation is translated to
 *  base_value..base_value+size-1 or to actual_value[]+base_value
 */

#define TAKEN_BIT_MASK     (1 << (sizeof(int) * 8 - 1))
#define ERR_BIT_MASK       (1 << (sizeof(int) * 8 - 2))
#define TAKE_ERR_BIT_MASK  (ERR_BIT_MASK | TAKEN_BIT_MASK)

#define IsTaken(k)         (vec[k] < 0)
#define Take(k)            (vec[k] |= TAKEN_BIT_MASK)
#define Assign0(k, v)      (vec[k] |= (v)) /* in the case we know the vec[k] = 0 (or 0X8000...0) */
#define Assign(k, v)       (vec[k] = (vec[k] & TAKE_ERR_BIT_MASK) | (v))
#define Value(k)           (vec[k] & ~TAKE_ERR_BIT_MASK)

#define IsError(k)         ((vec[k] & ERR_BIT_MASK) != 0)
#define SetError(k)        (vec[k] |= ERR_BIT_MASK)


#if 0
#define PERMUT_MAX_TRIES    (size)
#endif

void
Random_Permut(int *vec, int size, const int *actual_value, int base_value)
{
  int i, v;

  /* step 1: be sure nothing is taken (reset all bit signs) */

  memset(vec, 0, sizeof(int) * size); 

#ifdef PERMUT_MAX_TRIES
  int tries = 0;
#endif

  /* step 2: init vec[] with a permutation of 0..size-1 (all 'take' marks are reset) */

  for(i = 0; i < size; i++)
    {
      do
	v = Random(size);
      while(IsTaken(v)
#ifdef PERMUT_MAX_TRIES
	    && ++tries < PERMUT_MAX_TRIES
#endif
	    );
    
#ifdef PERMUT_MAX_TRIES
      if (tries >= PERMUT_MAX_TRIES)
	{			
	  int n = Random(size - i);

	  for(v = 0; ; v++)	/* find the Nth unused value */
	    {
	      if (!IsTaken(v))
		{
		  if (n == 0)
		    break;
		  n--;
		}
	    }
	}
      tries = 0;
#endif

      /* here v is the random value (in 0..size-1) for vec[i] */
      
      Assign0(i, v);
      Take(v);      /* mark found value v */
    }
  

  /* step 3: reset marks and convert to actual_value[] (if any) + base_value */

  if (actual_value == NULL)
    {				/* reset taken and re-add base_value */
      for(i = 0; i < size; i++)
	vec[i] = Value(i) + base_value; 
    }
  else
    {				/* reset taken and set to actual_value[] + base_value */
      for(i = 0; i < size; i++)
	vec[i] = actual_value[Value(i)] + base_value; 
    }
}




/*
 *  RANDOM_PERMUT_REPAIR
 *
 *  Repair a vector of size elements containing a random permutation 
 *  of values in base_value..base_value+size-1.
 *
 *  All erroneous elements are first marked (bit just before the sign bit) 
 *  and then fixed (as in Random_Permut() see above).
 *
 *  At the end, all sign+error bits are reset.
 */

void
Random_Permut_Repair(int *vec, int size, const int *actual_value, int base_value)
{
  int i, j, v;
  int nb_err = 0;

#ifdef PERMUT_MAX_TRIES
  int tries = 0;
#endif

  /* step 1: transform all values of vec[] to values in 0..size-1  (all 'take' marks are reset) */

  if (actual_value == NULL)
    {
      for(i = 0; i < size; i++)
	{
	  v = vec[i] - base_value;
	  if (v < 0 || v >= size) /* error: set it to size; will be detected as an error */
	      v = size;
	  vec[i] = v;
	}
    }
  else
    {
      for(i = 0; i < size; i++)
	{
	  v = vec[i] - base_value;
	  for(j = 0; j < size && v != actual_value[j]; j++)
	    ;
	  vec[i] = j;		/* if not found j = size; will be detected as an error */
	}
    }

  /* step 2: detect errors in the permutation */

  for(i = 0; i < size; i++)
    {
      v = Value(i);

      if (v < 0 || v >= size)
	{
	  SetError(i);
	  nb_err++;
	  continue;
	}

      /* if v is taken it is an error except if actual_value[v] appears several times
       * (since repeated elements must be grouped thus the next one is at v+1)
       * then replace all follwing refs to v by v+1 (and v becomes v+1)
       */

      if (IsTaken(v))
        {
	  int v0 = v++;
	  if (actual_value == NULL || v >= size || actual_value[v0] != actual_value[v])
	    {
	      SetError(i);	/* error at i */
	      nb_err++;
	      continue;
	    }

	  for(j = i; j < size; j++)
	    if (Value(j) == v0)
	      Assign(j, v);
	}
      
      Take(v);
    }

  /* step 3: fix the errors in the permutation */

  for(i = 0; nb_err; i++)
    {
      if (IsError(i))
	{
	  do
	    v = Random(size);
	  while(IsTaken(v)
#ifdef PERMUT_MAX_TRIES
		&& ++tries < PERMUT_MAX_TRIES
#endif
		);
    
#ifdef PERMUT_MAX_TRIES
	  if (tries >= PERMUT_MAX_TRIES)
	    {			
	      int n = Random(nb_err);

	      for(v = 0; ; v++)	/* find the Nth unused value */
		{
		  if (!IsTaken(v))
		    {
		      if (n == 0)
			break;
		      n--;
		    }
		}
	    }
	  tries = 0;
#endif

	  /* here v is the random value (in 0..size-1) for vec[i] */
      
	  Assign(i, v); 
	  Take(v);      /* mark found value v */
	  nb_err--;
	}
    }

  /* step 3: reset marks and convert to actual_value[] (if any) + base_value */

  if (actual_value == NULL)
    {				/* reset taken and re-add base_value */
      for(i = 0; i < size; i++)
	vec[i] = Value(i) + base_value; 
    }
  else
    {				/* reset taken and set to actual_value[] + base_value */
      for(i = 0; i < size; i++)
	vec[i] = actual_value[Value(i)] + base_value; 
    }
}



/*
 *  RANDOM_PERMUT_CHECK
 *
 *  Check if a vector of size elements is a permutation of
 *  values in base_value..base_value+size-1.
 *
 *  Returns -1 if OK or the index of a found error (maybe not 
 *  the first if case some values are out the valid range
 */
int
Random_Permut_Check(int *vec, int size, const int *actual_value, int base_value)
{
  int i, j, v;
  int ret = -1;


  /* step 1: transform all values of vec[] to values in 0..size-1 */

  if (actual_value == NULL)
    {
      for(i = 0; i < size; i++)
	{
	  v = vec[i] - base_value;
	  if (v < 0 || v >= size) /* error */
	    {
	      if (base_value != 0)
		for(j = 0; j < i; j++)	/* restore: re-add base_value */
		  vec[j] += base_value;

	      return i;
	    }
	  vec[i] = v;
	}
    }
  else
    {
      for(i = 0; i < size; i++)
	{
	  v = vec[i] - base_value;
	  for(j = 0;; j++)
	    {
	      if (j >= size)	/* not found: error */
		{
		  for(j = 0; j < i; j++)	/* restore: replace index by values */
		    vec[j] = actual_value[vec[j]] + base_value;

		  return i;
		}
	      if (actual_value[j] == v)
		break;
	    }

	  vec[i] = j;		/* found: replace value by index in actual_value[] */
	}
    }

  /* step 2: check nothing is taken more than once */

  for(i = 0; i < size; i++)
    {
      v = Value(i);

      /* if v is taken it is an error except if actual_value[v] appears several times
       * (since repeated elements must be grouped thus the next one is at v+1)
       * then replace all follwing refs to v by v+1 (and v becomes v+1)
       */

      if (IsTaken(v))
        {
	  int v0 = v++;
	  if (actual_value == NULL || v >= size || actual_value[v0] != actual_value[v])
	    {
	      ret = i;		/* error at i */
	      break;
	    }

	  for(j = i; j < size; j++)
	    if (Value(j) == v0)
	      Assign(j, v);
        }
      
      Take(v);
    }

  /* step 3: remove 'take' marks and restore initial values */

  if (actual_value == NULL)
    {				/* reset taken and re-add base_value */
      for(i = 0; i < size; i++)
	vec[i] = Value(i) + base_value; 
    }
  else
    {				/* reset taken and set to actual_value[] + base_value */
      for(i = 0; i < size; i++)
	vec[i] = actual_value[Value(i)] + base_value; 
    }

  return ret;
}


#ifdef USE_ALONE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"

#define SIZE  50
#define BASE_VALUE  -10

int vec1[SIZE];
int vec2[SIZE];
int vec3[SIZE];

#if 0
#define actual_value NULL
#else
int actual_value[SIZE];


int CompareInt(const void *x, const void *y)
{
  return * (int *) x - * (int *) y;
}
#endif



int
main(int argc, char *argv[])
{
  int i;
  int seed;

  if (argc >= 2)
    seed = atoi(argv[1]);
  else
    {
      Randomize();
      seed = Random(65536);
    }
  Randomize_Seed(seed);

  printf("seed: %d\nbase_value: %d\n", seed, BASE_VALUE);

#ifndef actual_value
  for(i = 0; i < SIZE; i++) {
    actual_value[i] = Random(SIZE) - (SIZE/3);
  }
  qsort(actual_value, SIZE, sizeof(int), CompareInt);

  printf("actual_value + base_value:\n");
  for(i = 0; i < SIZE; i++) {
    printf("%4d  ", actual_value[i] + BASE_VALUE);
  }
  printf("\n\n");
#endif

  Random_Permut(vec1, SIZE, actual_value, BASE_VALUE);

  printf("initial permutation\n");
  for(i = 0; i < SIZE; i++)
    printf("%4d  ", vec1[i]);
  printf("\n\n");

  i = Random_Permut_Check(vec1, SIZE, actual_value, BASE_VALUE);
  if (i >= 0)
    {
      printf("ERROR at elem %d = %d\n", i, vec1[i]);
      return 1;
    }

  memcpy(vec2, vec1, sizeof(vec1));

  for(i = 0; i < SIZE / 2; i++)
    vec2[Random(SIZE)] = Random(SIZE * 2) - SIZE;

  printf("\nchanging some values randomly to break the initial permutation:\n");

  vec2[12] = -18;
  vec2[6] = -19;

  for(i = 0; i < SIZE; i++)
    printf("%4d%s ", vec2[i], (vec1[i] != vec2[i]) ? "*" : " ");
  printf("\n");

  i = Random_Permut_Check(vec2, SIZE, actual_value, BASE_VALUE);
  if (i >= 0)
    printf("non permut elem %d = %d\n\n", i, vec2[i]);


  memcpy(vec3, vec2, sizeof(vec3));

  Random_Permut_Repair(vec3, SIZE, actual_value, BASE_VALUE);

  printf("repaired permutation\n");
  for(i = 0; i < SIZE; i++)
    printf("%4d%s ", vec3[i], (vec3[i] != vec2[i]) ? "*" : " ");
  printf("\n\n");

  i = Random_Permut_Check(vec3, SIZE, actual_value, BASE_VALUE);
  if (i >= 0)
    {
      printf("ERROR at elem %d = %d\n", i, vec3[i]);
      return 1;
    }

  printf("OK\n");
  return 0;
}

#endif /* USE_ALONE */
