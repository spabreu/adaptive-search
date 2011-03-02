#ifndef _QAP_UTILS
#define _QAP_UTILS

#include "tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef No_Gcc_Warn_Unused_Result
#define No_Gcc_Warn_Unused_Result(t) if(t)
#endif


typedef int *QAPVector;
typedef int **QAPMatrix;


/* 
 * use the following cmd in the QA data directory to generate the fields of the structure
 * for i in `cut -f 1 -d ' ' *.qap | sort | uniq`; do printf '  int %s;\n' $i;done
 */

typedef struct {
  //  char file_name[128];

  int size;			/* size of the problem (always known) */
  int opt;			/* optimal cost (0 if unknown) */
  int bound;			/* best bound (0 if unknown) */
  int bks;			/* best known solution cost (0 if unknown) */

  QAPMatrix a;			/* flow matrix */
  QAPMatrix b;			/* distance matrix */
} QAPInfo;



QAPMatrix
QAP_Alloc_Matrix(int n)
{
  n++;
  QAPMatrix mat = malloc(n * sizeof(mat[0]));
  int i;

  if (mat == NULL)
    {
      fprintf(stderr, "%s:%d malloc failed\n", __FILE__, __LINE__);
      exit(1);
    }

  for(i = 0; i < n; i++)
    {
      mat[i] = malloc(n * sizeof(mat[0][0]));
      if (mat[i] == NULL)
	{
	  fprintf(stderr, "%s:%d malloc failed\n", __FILE__, __LINE__);
	  exit(1);
	}
    }
  return mat;
}

void
QAP_Free_Matrix(QAPMatrix mat, int n)
{
  int i;
  for(i = 0; i < n; i++)
    free(mat[i]);

  free(mat);
}



#define QAP_Alloc_Vector(n)            malloc((n + 1) * sizeof(int))

#define QAP_Free_Vector(v)             free(v)


#define QAP_Copy_Vector(dst, src, n)   memcpy((dst), (src), (n) * sizeof(int))


void
QAP_Read_Matrix(FILE *f, int n, QAPMatrix *pm)
{
  *pm = QAP_Alloc_Matrix(n);
  int i, j;

  for(i = 0; i < n; i++)
    for(j = 0; j < n; j++)
      if (fscanf(f, "%d", &((*pm)[i][j])) != 1)
	{
	  fprintf(stderr, "error while reading matrix at [%d][%d]\n", i, j);
	  exit(1);
	}
}


/*
 *  Load a QAP problem
 *
 *  file_name: the file name of the QAP problem (can be a .dat or a .qap)
 *  qi: the ptr to the info structure (can be NULL)
 *      the matrix a and b are not allocated if the ptr != NULL at entry !
 *
 *  Returns the size of the problem
 */
int
QAP_Load_Problem(char *file_name, QAPInfo *qi)
{
  int n;
  FILE *f;

  if ((f = fopen(file_name, "rt")) == NULL) {
    perror(file_name);
    exit(1);
  }

  if (fscanf(f, "%d", &n) != 1)
    {
      fprintf(stderr, "error while reading the size\n");
      exit(1);
    }

  if (qi != NULL)		/* only need the size */
    {
      static char buff[1024];
      char *p = buff;
      int good_format = 1;
      int x[2];
      int nb_x = 0;
      int nb_params_expected = sizeof(x) / sizeof(x[0]);

      if (fgets(buff, sizeof(buff), f) == NULL)
	{
	  fprintf(stderr, "error while reading end of line\n");
	  exit(1);
	}

      for(;;)
	{
	  while(*p == ' ' || *p == '\t')
	    p++;

	  if (*p == '\r' || *p == '\n')
	    break;

	  if ((*p != '-' && !isdigit(*p)) || nb_x == nb_params_expected)
	    {
	      good_format = 0;
	      break;
	    }

	  x[nb_x++] = strtol(p, &p, 10);      
	}

      qi->size = n;

      if (good_format && nb_x == nb_params_expected)
	{
	  qi->opt = x[0];
	  qi->bks = x[1];
	  if (qi->opt < 0)
	    {
	      qi->bound = -qi->opt;
	      qi->opt = 0;
	    }
	  else
	    qi->bound = qi->opt;
	} else
	qi->opt = qi->bks = qi->bound = 0;

      QAP_Read_Matrix(f, n, &qi->a);
      QAP_Read_Matrix(f, n, &qi->b);
    }

  fclose(f);
  
  return n;
}


void
QAP_Display_Vector(QAPVector sol, int n)
{
  int i;
  for(i = 0; i < n; i++)
    printf("%d ", sol[i]);
  printf("\n");
}


void
QAP_Display_Matrix(QAPMatrix mat, int n)
{
  int i, j;
  int max = 0;

  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      {
        if (mat[i][j] > max)
          max = mat[i][j];
      }

  int nb10 = 0;
  int max10 = 1;

  while(max >= max10)
    {
      nb10++;
      max10 *= 10;
    }

  for(i = 0; i < n; i++)
    {
      char *pref = "";
      for (j = 0; j < n; j++)
        {
          printf("%s%*d", pref, nb10, mat[i][j]);
          pref = " ";
        }
      printf("\n");
    }
}



#ifndef QAP_NO_MAIN

#ifndef DEFAULT_NB_ITERS
#define DEFAULT_NB_ITERS  10000
#endif

int seed = -1;
int nb_execs = 1;
int nb_iters = DEFAULT_NB_ITERS;
int read_initial;
int exchange;
char *file_name;

#ifdef NEEDS_ALGO_SPECIFIC_ARGS

char *algo_specific_args[10];

#endif

int Solve(QAPInfo *qi, QAPVector sol, int nb_iters);

void QAP_Parse_Cmd_Line(int argc, char *argv[]);


/*
 *  MAIN
 *
 */

int
main(int argc, char *argv[])
{
  int no_exec;
  double somme_sol = 0.0;
  QAPInfo qap_info;
  int n, i;

  QAP_Parse_Cmd_Line(argc, argv);
  
  if (seed < 0)
    seed = Randomize();
  else
    Randomize_Seed(seed);

  setlinebuf(stdout);
  //setvbuf(stdout, NULL, _IOLBF, 0);

  n = QAP_Load_Problem(file_name, &qap_info);
  if (exchange)
    {
      QAPMatrix tmp = qap_info.a;
      qap_info.a = qap_info.b;
      qap_info.b = tmp;
    }

  QAPVector sol = QAP_Alloc_Vector(n);

  printf("used seed: %d\n", seed);
  if (exchange)
    printf("matrix are exchanged\n");
  printf("QAP read infos: ");
  printf(" size:%d ", qap_info.size);
  if (qap_info.opt > 0)
    printf(" opt: %d ", qap_info.opt);
  else if (qap_info.bound > 0)
    printf(" bound: %d ", qap_info.bound);
  if (qap_info.bks > 0)
    printf(" bks: %d", qap_info.bks);
  printf("\n");
  
  for (no_exec = 1; no_exec <= nb_execs; no_exec++)
    {
      if (!read_initial)
	Random_Permut(sol, n, NULL, 0);
      else
	{
	  int based_1 = 1;
	  printf("enter the initial configuration:\n");
	  for(i = 0; i < n; i++)
	    {
	      No_Gcc_Warn_Unused_Result(scanf("%d", &sol[i]));
	      if (sol[i] == 0)
		based_1 = 0;
	    }
	  getchar();                /* the last \n */
	  if (based_1) 	  
	    printf("entered solution is 1-based\n");
	  i = Random_Permut_Check(sol, n, NULL, based_1);
	  if (i >= 0)
	    {
	      fprintf(stderr, "not a valid permutation, error at [%d] = %d\n", i, sol[i]);
	      Random_Permut_Repair(sol, n, NULL, based_1);
	      printf("possible repair:\n");
	      QAP_Display_Vector(sol, n);
	      exit(1);
	    }
	  if (based_1)
	    for(i = 0; i < n; i++)
	      sol[i]--;
	}

      int cost = Solve(&qap_info, sol, nb_iters);

      printf("%d Solution found:\n", cost); QAP_Display_Vector(sol, n);
      somme_sol += cost;
    }

  if (qap_info.opt > 0)
    printf("Average cost: %f, average dev: %f\n",
	   somme_sol / nb_execs, 100 * (somme_sol / nb_execs - qap_info.opt) / qap_info.opt);

  return 0;
}



#define L(...) do { fprintf(stderr,  __VA_ARGS__); fprintf(stderr, "\n"); } while(0)

/*
 *  PARSE_CMD_LINE
 *
 */
void
QAP_Parse_Cmd_Line(int argc, char *argv[])
{
  int i;

  nb_execs = 1;
  read_initial = 0;

  for(i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-')
	{
	  switch(argv[i][1])
	    {
	    case 'i':
	      read_initial = 1;
	      continue;

	    case 's':
	      if (++i >= argc)
		{
		  L("random seed expected");
		  exit(1);
		}
	      seed = atoi(argv[i]);
	      continue;

	    case 'b':
	      if (++i >= argc)
		{
		  L("nb_execs expected");
		  exit(1);
		}
	      nb_execs = atoi(argv[i]);
	      continue;

	    case 'a':
	      if (++i >= argc)
		{
		  L("nb_iters expected");
		  exit(1);
		}
	      nb_iters = atoi(argv[i]);
	      continue;

	    case 'x':
	      exchange = 1;
	      continue;

	    case 'h':
	      L("Usage: %s [ OPTION ] FILE_NAME", argv[0]);
	      L(" ");
	      L("   -i          read initial configuration");
	      L("   -s SEED     specify random seed");
	      L("   -b NB_EXECS execute NB_EXECS times");
	      L("   -a NB_ITERS abort after NB_ITERS iterations");
	      L("   -B          stop if BKS is reached");
	      L("   -x          exchange QAP matrix (A<->B)");
#ifdef NEEDS_ALGO_SPECIFIC_ARGS
	      L("   -0..-9 ARG  algo-specific argument 0");
#endif
	      L("   -h          show this help");
	      exit(0);

	    default:
#ifdef NEEDS_ALGO_SPECIFIC_ARGS
	      if (algo_specific_args != NULL && argv[i][1] >= '0' && argv[i][1] <= '9' && argv[i][2] == '\0')
		{
		  if (++i >= argc)
		    {
		      L("argument expected after %s", argv[i]);
		      exit(1);
		    }
		  algo_specific_args[argv[i][1] - '0'] = argv[i];
		  continue;
		}
#endif
	      fprintf(stderr, "unrecognized option %s (-h for a help)\n", argv[i]);
	      exit(1);
	    }
	}

      if (file_name == NULL)
	{
	  file_name = argv[i];
	}
      else
	{
	  fprintf(stderr, "unrecognized argument %s (-h for a help)\n", argv[i]);
	  exit(1);
	}
    }

  if (file_name == NULL)
    {
      fprintf(stderr, "QAP file name expected\n");
      exit(1);
    } 
}

#endif /* !QAP_NO_MAIN */


#endif /* _QAP_UTILS */

