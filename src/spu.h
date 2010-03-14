// == AS SPU back end =========================================================
//
// -- Salvador Abreu, 2009 ----------------------------------------------------
//

#ifndef __SPU_H
#define __SPU_H 1

#define CELL 1

#undef  AS_SPU
#undef  AS_PPU

#define AS_SPU

#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include <unistd.h>
#include <sys/types.h>

// -- public constants  -------------------------------------------------------

#define SPU_MAX_DMA (16 * 1024)


// -- public types ------------------------------------------------------------

#include "../cell-common.h"

typedef struct _asp_thread_d asp_thread_d;


// -- public functions --------------------------------------------------------

#if defined(DEBUG) && (DEBUG & 4)
#define DPRINTF(...) printf (__VA_ARGS__)
#else
#define DPRINTF(...) ((void) 0)
#endif

void as_init (uint64_t speid, uint64_t argp);
void as_exit (int status);

void as_receive ();
void as_send    ();

#include "../cell-extern.h"

#define as_exitf(...)				\
  do {						\
    printf (__VA_ARGS__);			\
    as_exit (1);				\
  } while (0)


//void   as_stop  (asp_thread_d *arg);


// -- public globals ----------------------------------------------------------

spu_data_t data;		// buffer for DMA
uint32_t   tag;			// MFC tag to sync DMA

// -- internal types ----------------------------------------------------------



#endif // __SPU_H
