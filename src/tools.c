
/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *
 *  tools.c: utilities
 */


#include <string.h>
#include <time.h>

#if defined(__unix__) || defined(__CYGWIN__)
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#endif

#include "tools.h"
#include "stdlib.h"                        /* random() */

#if defined BACKTRACK
#include <string.h>                        /* memcpy() */
#endif /* BACKTRACK */

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
static unsigned int	last;
static unsigned int	a;
static unsigned int	c;


/*------------*
 * Prototypes *
 *------------*/

#if defined PRINT_COSTS
/* Read n entries */
int
readn( int fd, char * buffer, int n )
{
  int nread;
  int nleft;
  char * ptr;

  ptr = buffer ;
  nleft = n;
  while( nleft != 0 ) {
    if( (nread = read(fd, ptr, nleft)) < 0 ) {
      if( nread < 0 )       // ERROR
	switch(errno) {
	case EBADF:
	  printf("File descriptor unvalid or not open for reading") ;
	  return 0 ;
	case EFAULT:
	  printf("Buffer pointe en dehors de l'espace d'adressage") ;
	  return 0 ;
	case EINVAL:
	  printf("EINVAL") ;
	  return 0 ;
	case EIO:
	  printf("EIO") ;
	  return 0 ;
	default:
	  printf("Undefined") ;
	  return 0 ;
	}
	return( 0 ) ;
    } else if( nread == 0 ) /* EOF */
      break ;
    nleft -= nread;
    ptr += nread;
  }
  return( n-nleft ) ;
}
/* Write n entries */
size_t
writen( int fd, const char * buffer, size_t n )
{
  size_t nleft;
  size_t nwritten;
  const char * ptr;
  
  ptr = buffer ;
  nleft = n ;
  while( nleft > 0 ) {
    if( (nwritten = write(fd, ptr, nleft)) <= 0 ) {
      if( errno == EINTR )
	nwritten = 0 ;	/* and call write() again */
      else
	return( 0 ) ;	/* error */
    }
    nleft -= nwritten ;
    ptr   += nwritten ;
  }
  return(n) ;
}
#endif /* PRINT_COSTS */

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

#elif defined(__CYGWIN__)	/* does not work well, returns real_time */

  struct tms time_buf1;

  times(&time_buf1);
  user_time = time_buf1.tms_utime * 1000 / tps;

#elif defined(_WIN32)

  user_time = (long) ((double) clock() * 1000 / CLOCKS_PER_SEC);

#else

  user_time = 0;
#warning User_Time not available

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
#warning Real_Time not available

#endif

  return real_time - start_real_time;
}




/*
 *  RANDOMIZE_SEED
 *
 *  Initializes the random number generator with a given seed.
 */
void
Randomize_Seed(unsigned seed)
{
  srandom(seed);
}



/*
 *   RANDOM_DOUBLE
 *
 *   Returns a random real number in [0..1) (1 not included)
 */
double
Random_Double(void)
{
  return ((double) random() / (RAND_MAX + 1.0));
}


/**
 * randChaos
 *
 * Take an integer n and set "last" to 
 * a random value in [0..max-1].
 *
 * This function is actually a one-dimensional
 * chaotic map using parameters a and c as well 
 * as constants DELTA_A and DELTA_C
 *
 */
int randChaos(int max, int *var_last, int *var_a, int *var_c)
{
  int tmp;

  tmp = (*var_a * *var_last + *var_c) % max;
  // Offset of 16 since we use 32-bit integer
  // Good results with offset = (bit length / 2)
  *var_last = tmp ^ (tmp >> OFFSET);
  *var_a = (*var_a + DELTA_A) % max;
  *var_c = (*var_c + DELTA_C) % max;

  return *var_last;
}


/**
 * randChaosInit
 *
 * Initialise static variables used 
 * by randChaosDouble
 *
 */
void randChaosInit(void)
{
  srandom(time(NULL));
  last = Random(UINT_MAX);
  a = 5;
  c = 1;
}


/**
 * randChaosDouble
 *
 * Returns a random value in [0..1[.
 *
 * This function is actually a one-dimensional
 * chaotic map using parameters a and c as well 
 * as constants DELTA_A and DELTA_C
 *
 * MAKE SURE randChaosInit() HAS BEEN CALLED FIRST!
 *
 */
double randChaosDouble(void) 
{
  unsigned int tmp;

  tmp = (a * last + c) % UINT_MAX;
  last = tmp ^ (tmp >> OFFSET);
  a = (a + DELTA_A) % UINT_MAX;
  c = (c + DELTA_C) % UINT_MAX;

  return ((double)last) / UINT_MAX;  
}


/**
 * randChaosInt
 *
 * Returns a random value in [0..n-1].
 *
 */
unsigned int randChaosInt(unsigned int n)
{
  return (unsigned int) (randChaosDouble() * n);
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
  static int count = 0;
  unsigned seed;
  int seed_found = 0;
#if defined(__unix__)
  {
    int fd;
    if ((fd = open("/dev/urandom", O_RDONLY|O_NONBLOCK)) != -1 ||
	(fd = open("/dev/random",  O_RDONLY|O_NONBLOCK)) != -1)
      {
	struct stat st;
	/* Make sure it's a character device */
	if ((fstat(fd, &st) == 0) && S_ISCHR(st.st_mode) && read(fd, &seed, sizeof(seed)) == sizeof(seed))
	  seed_found = 1;
	close(fd);
      }
  }
#endif
  if (!seed_found)
    {
#if defined(__unix__) || defined(__CYGWIN__)
      struct timeval tv;
      unsigned seed;
      gettimeofday(&tv, NULL);
      seed = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
#else
      seed = GetTickCount();
#endif
      count = (count + random()) % 0xFFFFFFF;
      seed = seed + (getpid() << (seed & 0xFF)) + getppid();
      seed *= count;
    }
  
  seed = seed & 0x7FFFFFFF;

  Randomize_Seed(seed);

  return seed;
}




/*
 *  RANDOM
 *
 *  Returns a random number in [0..n-1].
 */
unsigned
Random(unsigned n)
{
  return (unsigned) (Random_Double() * n);
}



/*
 *  RANDOM_INTERVAL
 *
 *  Returns a random number in [inf..sup]
 */
int
Random_Interval(int inf, int sup)
{
  return (int) (Random_Double() * (sup - inf + 1)) + inf;
}

/*
 *  RANDOM_ARRAY_PERMUT
 *
 *  Generate a random permutation of a given vector of size elements.
 *
 *  Use the following shuffle (Durstenfeld implementation of Fisher-Yates shuffle)
 *
 *  To shuffle an array a of n elements:
 *  for(i = size   1; i >= 1; i--) {
 *    j = random number in [0..i]
 *    swap vec[i] and vec[j]
 *  }
 */

void
Random_Array_Permut(int *vec, int size)
{  
  int i, j, z;

  for(i = size - 1; i > 0; i--)
    {
      j = Random(i + 1);
      z = vec[i];
      vec[i] = vec[j];
      vec[j] = z;
    }
}


/*
 *  RANDOM_PERMUT
 *
 *  Generate a vector of size elements with a random permutation 
 *  - of values in base_value..base_value+size-1 (if actual_value == NULL)
 *  - of values in actual_value[] + base_value
 *
 *  Use the following shuffle (Durstenfeld implementation of Fisher-Yates shuffle)
 *
 *  vec[0] = source[0];
 *  for(i = 1; i < size; i++) {
 *    j = random number in [0..i]
 *    vec[i] = vec[j];
 *    vec[j] = source[i];
 *  }
 */

void
Random_Permut(int *vec, int size, const int *actual_value, int base_value)
{
#if 0
  int i, j, k, z;

  if (actual_value == NULL)
    {
      for(i = 0; i < size; i++)
	vec[i] = base_value + i;
    }
  else
    {
      for(i = 0; i < size; i++)
	vec[i] = actual_value[i] + base_value;
    }


  for(i = 0; i < size; i++)
    {
      j = Random(size);
      k = Random(size);
      z = vec[j];
      vec[j] = vec[k];
      vec[k] = z;
    }
#else

  int i, j;

  if (actual_value == NULL)
    {
      vec[0] = base_value;
      for(i = 1; i < size; i++)
	{
	  j = Random(i + 1);
	  vec[i] = vec[j];
	  vec[j] = base_value + i;
	}
    }
  else
    {
      vec[0] = base_value + actual_value[0];
      for(i = 1; i < size; i++)
	{
	  j = Random(i + 1);
	  vec[i] = vec[j];
	  vec[j] = base_value + actual_value[i];;
	}
    }
#endif
}




/*
 *  RANDOM_PERMUT_REPAIR
 *
 *  Repair a vector of size elements containing a random permutation 
 *  - of values in base_value..base_value+size-1 (if actual_value == NULL)
 *  - of values in actual_value[] + base_value
 *
 *  All erroneous elements are first marked (bit just before the sign bit) 
 *  and then fixed.
 *
 *  To have no problem with negative numbers (base_value < 0 and/or
 *  negative values un actual_value[]) the repair of the permutation
 *  is done in 0..size-1
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
 *  At the end, all sign+error bits are reset and the permutation is 
 *  translated to base_value..base_value+size-1 or to actual_value[]+base_value
 */

#define TAKEN_BIT_MASK     (1 << (sizeof(int) * 8 - 1))
#define ERR_BIT_MASK       (1 << (sizeof(int) * 8 - 2))
#define TAKEN_ERR_BIT_MASK (ERR_BIT_MASK | TAKEN_BIT_MASK)

#define IsTaken(k)         (vec[k] < 0)
#define SetTaken(k)        (vec[k] |= TAKEN_BIT_MASK)
#define Assign(k, v)       (vec[k] = (vec[k] & TAKEN_ERR_BIT_MASK) | (v))
#define Value(k)           (vec[k] & ~TAKEN_ERR_BIT_MASK)

#define IsError(k)         ((vec[k] & ERR_BIT_MASK) != 0)
#define SetError(k)        (vec[k] |= ERR_BIT_MASK)


#if 0
#define PERMUT_MAX_TRIES   (size)
#endif


void
Random_Permut_Repair(int *vec, int size, const int *actual_value, int base_value)
{
  int i, j, v;
  int nb_err = 0;

#ifdef PERMUT_MAX_TRIES
  int tries = 0;
#endif

  /* step 1: transform all values of vec[] to values in 0..size-1  (all 'taken' marks are reset) */

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
      
      SetTaken(v);
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
	  SetTaken(v);      /* mark found value v */
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
 *  - of values in base_value..base_value+size-1 (if actual_value == NULL)
 *  - of values in actual_value[] + base_value
 *
 *  Returns -1 if OK or the index of a found error (maybe not the first)
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
	  for(j = 0; ; j++)
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
       * then replace all following refs to v by v+1 (and v becomes v+1)
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
      
      SetTaken(v);
    }

  /* step 3: remove 'taken' marks and restore initial values */

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

#define SIZE  30
#define BASE_VALUE  1//-10

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


#if (defined BACKTRACK) || (defined COMM_CONFIG)

void split(long long int tooLong, unsigned int *high, unsigned int *low)
{
  *high = (int)(tooLong >> 32);
  *low = (int)(tooLong & 0xFFFFFFFF);  
}

long long int concat(unsigned int high, unsigned int low)
{
  return ((long long int)high << 32 | low);
}

void
queue_configuration( backtrack_configuration * item )
{
  memcpy( &(Gbacktrack_array[Gbacktrack_array_end].solution),
	  item->solution,
	  Gconfiguration_size_in_bytes) ;
  /* YC->all: todo manage the rest of structure */

  Gbacktrack_array_end++ ;
  if( Gbacktrack_array_end == SIZE_BACKTRACK )
    Gbacktrack_array_end = 0 ;

  if( Gbacktrack_array_end == Gbacktrack_array_begin ) {
    Gbacktrack_array_begin++ ;
    if( Gbacktrack_array_begin == SIZE_BACKTRACK )
      Gbacktrack_array_begin = 0 ;
  }
}
backtrack_configuration *
unqueue_configuration()
{
  backtrack_configuration * item ;

  if( Gbacktrack_array_begin == Gbacktrack_array_end )
    return NULL ;

  /* YC->all: we can directly manage (permutations/reset/restart) here... */
  item = malloc(sizeof(backtrack_configuration)) ;
  item->solution = malloc(Gconfiguration_size_in_bytes) ;
  Gbacktrack_array_end-- ;
  if( Gbacktrack_array_end < 0 )
    Gbacktrack_array_end = SIZE_BACKTRACK-1 ;

  memcpy( item->solution,
	  &(Gbacktrack_array[Gbacktrack_array_end].solution),
	  Gconfiguration_size_in_bytes ) ;
  
  /* YC->all: manage the rest of structure -- permutations, etc. */
  return(item) ; 
}
#endif /* BACKTRACK || COMM_CONFIG */
