// == AS PPU front end ========================================================
//
// -- Salvador Abreu, 2009 ----------------------------------------------------
//

#include "ppu.h"
#include <fcntl.h>
#include <malloc.h>
#include <string.h>

// -- internal types ----------------------------------------------------------

// -- global variables --------------------------------------------------------

extern int nb_threads;
extern spe_program_handle_t *bench_spu;

volatile spu_data_t sds[NUM_SPES] __attribute__ ((aligned (128)));
volatile uint32_t as_finished     __attribute__ ((aligned (16)));
int as_found_in = -1;

int as_pipe[2];
int as_alive[NUM_SPES];


// -- as_thread () ------------------------------------------------------------
// SPU overseer thread main routine

static void *as_thread (void *arg)
{
  spu_data_t *sd = (spu_data_t *) arg;
  uint32_t entry = SPE_DEFAULT_ENTRY;
	
  DPRINTF ("as_thread: arg=0x%lx\n", (unsigned long) arg);


  /* the following pthread_cancel_asynchronous makes it possible to
   * restart threads avoiding 'spu_create(): File exists' errors
   * (errno = EEXIST) at subsequent spe_context_create (some errors
   * remain however - FIXED wit pthread_join)
   */
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  if (spe_context_run (sd->ctx, &entry, 0, sd, NULL, NULL) < 0) {
    perror ("can't run SPU context");
    return (void *) 1;
  }

  // -- we finished, so we have a solution...
  // -- kill all the other threads, then. now... :)
  if (as_found_in == -1) {
    as_found_in = sd->num;
    write (as_pipe[1], &as_found_in, sizeof(as_found_in)); // should block!
    DPRINTF ("as_thread(%d): solution found here\n", as_found_in);
  }
  else {
    DPRINTF ("as_thread(%d): already found in thread %d\n", sd->num, as_found_in);
  }


#if 0
  for (i=0; i<nb_threads; ++i)
    if (i != as_found_in /*&& !sds[i].dead*/) {
      // -- cancel thread i
      DPRINTF ("as_thread: cancelling thread %d\n", i);
//    sds[i].dead = 1;
      pthread_cancel (sds[i].thr);
    }
  DPRINTF ("as_thread: done cancelling\n");
#endif

  pthread_exit (NULL);
  return NULL;
}

void as_init (spe_program_handle_t *program, AdData *ad)
{
  int i, j;
  int nt = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, -1);

  // -- the CBE plays hard ----------------------------------------------------
  if (sizeof(struct spu_data) % SD_MULTIPLE) {
    int new_pad =
      SD_MULTIPLE - ((sizeof (struct spu_data) - SD_PAD) % SD_MULTIPLE);

    fprintf (stderr,
	     "fatal: struct spu_data not multiple of 128 bytes - "
	     "#define SD_PAD %d\n", new_pad);
    exit (1);
  }

  // -- cap nb_threads --------------------------------------------------------
  if (nb_threads > nt) nb_threads = nt;

  // -- create a pipe for termination notification ----------------------------
  {
    int flags, i;

    if (pipe (as_pipe) < 0) {
      perror ("as_init: pipe");
      exit (1);
    }

    for (i=0; i<2; ++i) {
      int fd = as_pipe[i];

      flags = fcntl(fd, F_GETFL);
      flags &= ~O_NONBLOCK;
      fcntl(fd, F_SETFL, flags);
    }
  }

  // -- all unborn ------------------------------------------------------------
  for (i=0; i<NUM_SPES; ++i)
    as_alive[i] = 0;
  as_found_in = -1;

  // -- create SPE threads and set them up to run BENCH_SPU -------------------
  for (i=0; i<nb_threads; ++i) {

    // -- create context
    sds[i].num = i;
    sds[i].ctx = spe_context_create (SPE_MAP_PS, NULL);
    if (sds[i].ctx == NULL) {
      perror ("spe_context_create");
      exit (1);
    }

    // -- load program into context
    if (spe_program_load (sds[i].ctx, program)) {
      perror ("spe_program_load");
      exit (1);
    }

    // -- get LS effective address
    sds[i].spe_ea = spe_ls_area_get (sds[i].ctx);

    // -- initialize sds[i] data structures
    sds[i].size  = ROUND_UP_128(ad->size_in_bytes);
    sds[i].value = memalign (128, sds[i].size);
    if (sds[i].value == NULL) {
      fprintf (stderr, "%s:%d: malloc failed\n", __FILE__, __LINE__);
      exit(1);
    }
    sds[i].ad = *ad;
    sds[i].ad.sol = sds[i].value;
    memcpy(sds[i].ad.sol, ad->sol, ad->size_in_bytes);

#if defined(DEBUG) && (DEBUG&3)
    if (sds[i].ad.do_not_init) {
      printf (">>>>>>>>> data to send (do_not_init=1):\n");
      Ad_Display(sds[i].ad.sol, (int *) &sds[i].ad, NULL);
      printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    }
#endif

    // -- map each SPE's MFC to main memory
    if ((sds[i].mbx_ea = spe_ps_area_get (sds[i].ctx, SPE_CONTROL_AREA)) == NULL) {
      perror ("Failed mapping MFC control area");
      exit (1);
    }
  }

  // -- setup topology: link mailboxes and LS addresses -----------------------
  for (i=0; i<nb_threads; ++i) {
    sds[i].mbx_nxt_ea = sds[(i+1) % nb_threads].mbx_ea;
    for (j=0; j<NUM_SPES; ++j) {
      sds[i].mbx_mfc_ea[j] = (j > nb_threads) ? 0: sds[j].mbx_ea;
      sds[i].spe_ls_ea[j] = sds[i].spe_ea;
    }
    sds[i].mbx_mfc_ea[i] = 0;
  }

  // -- all set; create pthreads to start each SPU ----------------------------
  for (i=0; i<nb_threads; ++i) {
    // -- create thread and start program
    if (pthread_create ((pthread_t *)&sds[i].thr, NULL, &as_thread, (void *) &sds[i]))  {
      perror ("pthread_create");
      exit (1);
    }

    // -- remember it exists
    as_alive[i] = (int) sds[i].thr;

    DPRINTF ("\nthread %d:\n", i);
    DPRINTF (" sd = &sds[%d] = 0x%x\n", i, (unsigned int) &sds[i]);
    DPRINTF ("  .value = %p\n", sds[i].value);
    DPRINTF ("  .num   = %d\n", sds[i].num);
    DPRINTF ("  .ctx   = %p\n", sds[i].ctx);
    DPRINTF ("  .thr   = %p\n", (void *) sds[i].thr);
    //    DPRINTF (" .argp = 0x%lx\n", sds[i].argp);
    //    DPRINTF (" .ea_base = 0x%llx\n", sds[i].ea_base);
    //    DPRINTF (" .ea_status = 0x%llx\n", sds[i].ea_status);
  }
}


int as_wait ()
{
  int as_found_in_buffer;
  const int sz = sizeof (as_found_in_buffer);

  if (read (as_pipe[0], &as_found_in_buffer, sz) != sz) {
    perror ("as_wait: read(pipe)");
    exit (1);
  }
  close (as_pipe[0]);

  return (as_found_in_buffer);
}

void as_kill (int index)
{
  if (index < nb_threads && sds[index].ctx && as_alive[index]) {
    as_alive[index] = 0;
    pthread_cancel (sds[index].thr);
    if (pthread_join (sds[index].thr, NULL)) {
      perror("Failed pthread_join");
      exit (1);
    }    
    if (spe_context_destroy ((void *) sds[index].ctx) != 0) {
      perror("spe_context_destroy");
      exit (1);
    }
  }
}

void as_killnot (int index)
{
  int i;
  for (i = 0; i < nb_threads; ++i)
    if (i != index && as_alive[i])
      as_kill (i);

  if (pthread_join (sds[index].thr, NULL)) {
    perror("Failed pthread_join");
    exit (1);
  }    
}



// -- Solve() stub ------------------------------------------------------------
//
// Function has the same interface as Solve() but will be used instead.
//
// input: AdData block (sd)
// output: -
// action:
//   - (1) for the ``nb_threads'' different threads, do:
//   - (2.1) copy *sd to a newly allocated 128-byte aligned block
//   - (2.2) copy value vector (variable size...) to another block
//   - (2.3) include address of (2.2) in (2.1) sd.
//   - (2.4) create a new SPE thread and pass it ``sd''
// these are done separately to allow for the SPE to allocate a variable-sized
// block which will accomodate the value vector

void SolveStub (AdData *ad)
{
  as_init (bench_spu, ad);
  as_wait ();

  if (as_found_in >= 0) {
    DPRINTF ("solution found by SPE %d\n", as_found_in);
    *ad = sds[as_found_in].ad;
  }

  as_killnot (as_found_in);
}
