// == AS PPU front end ========================================================
//
// -- Salvador Abreu, 2009 ----------------------------------------------------
//

#ifndef __PPU_H
#define __PPU_H 1

#define CELL 1

#undef  AS_SPU
#undef  AS_PPU

#define AS_PPU 1

#include <libspe2.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <cbe_mfc.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

// == public types ------------------------------------------------------------

#include "cell-common.h"


// -- public functions --------------------------------------------------------

#if defined(DEBUG) && (DEBUG & 4)
#define DPRINTF(...) printf (__VA_ARGS__)
#else
#define DPRINTF(...) ((void) 0)
#endif

void  as_init    (spe_program_handle_t *program, AdData *ad);

void  as_start   (spu_data_t *arg);
void  as_stop    (spu_data_t *arg);

int   as_wait    ();		// returns solution thread #
void  as_kill    (int index);	// kill this thread #
void  as_killnot (int index);	// kill all but this thread #

void  as_send    (spu_data_t *arg, void *data);


// -- public globals ----------------------------------------------------------

extern int as_found_in;
extern int as_nthreads;


#endif
