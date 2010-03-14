// == AS SPU/PPU common stuff =================================================
//
// -- Salvador Abreu, 2009 ----------------------------------------------------

#ifndef __CELL_COMMON_H_
#define __CELL_COMMON_H_ 1


// -- constants ---------------------------------------------------------------

#define NUM_SPES 16

// -- useful macros, etc ------------------------------------------------------

#define ROUND_UP_BITS(v,n) ((((v) + (1<<(n)) - 1) >> (n)) << (n))

#define ROUND_UP_128(v) ROUND_UP_BITS(v,7)
#define ROUND_UP_16(v)  ROUND_UP_BITS(v,4)


// -- definitions from the Adaptive Search solver -----------------------------

#include "ad_solver.h"
//#undef malloc

// -- the context that PPE forwards to SPEs -----------------------------------

#ifdef AS_SPU
typedef void *spe_context_ptr_t;
typedef void *pthread_t;
#endif

typedef struct spu_data spu_data_t;

#define SD_PAD 34
#define SD_MULTIPLE 128

struct spu_data {
  uint32_t          num;	// SPE & index number
  uint32_t          num_thr;	// number of threads and SPE contexts
  spe_context_ptr_t ctx;	// SPE execution context (inc. code)
  pthread_t         thr;	// associated pthread
				// -- stuff relevant in the SPE ---------------
  uint64_t          sd_ea;	// EA of *this* structure in the PPE
  int              *value;	// value vector address (in the PPE only)
  int               size;	// size in bytes of *value (multiple of 128)
				// -- info about SPEs -------------------------
  void             *mbx_ea;     // EA of SPE MFC (for mailboxes)
  void             *mbx_nxt_ea; // EA of SPE MFC of *NEXT* SPE (mailbox)
  void             *mbx_mfc_ea[NUM_SPES]; // as above but all of them!
  void             *spe_ea;	// EA of SPE LS (for DMAs)
  void             *spe_ls_ea[NUM_SPES]; // EA of SPE LS (for DMAs)
				// -- Adaptive Search data --------------------
  AdData            ad;		// parameters for AS
  char              pad[SD_PAD];// pad to SD_MULTIPLE bytes
};

#endif
