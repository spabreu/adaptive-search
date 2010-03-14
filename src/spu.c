// == AS SPU back end =========================================================
//
// -- Salvador Abreu, 2009 ----------------------------------------------------
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

// -- taken from SG247575_addmat_eitanp/simple_mailbox/spu/spu_mfcio_ext.h ----

//#include<libspe2.h>

#include "spu_mfcio_ext.h"

#include "spu.h"


// -- global variables --------------------------------------------------------

spu_data_t sd __attribute__ ((aligned (128))); // initialized with PPE data
int       *value;		// value vector, imported from PPE
uint32_t   tag = 0;		// our MFC tag for DMA ops


// -- private macros ----------------------------------------------------------

#define waittag(tag)				\
  do {                                          \
    mfc_write_tag_mask (1<<tag);		\
    mfc_read_tag_status_all ();                 \
  } while (0)



// -- main thread code --------------------------------------------------------

extern int Solve (AdData *ad);

int main (uint64_t speid, uint64_t argp)
{
  DPRINTF ("+(spu)main (%lld, %lld)\n", speid, argp);

  // -- reserve DMA tag ID for this SPU ---------------------------------------
  if ((tag = mfc_tag_reserve()) == MFC_TAG_INVALID)
    as_exitf ("ERROR - can't reserve a tag\n");
  DPRINTF (" [%lld] mfc_tag_reserve() = %d\n", speid, tag);

  // -- get CBE and problem information from system memory. -------------------
  DPRINTF (" [%lld] mfc_get (0x%x, 0x%llx, %d, %lu, 0, 0)\n", speid,
	   (unsigned) &sd, argp, sizeof(sd), (int) tag);
  mfc_getb (&sd, argp, sizeof(sd), tag, 0, 0);
  DPRINTF (" [%lld] waittag (%d)\n", speid, (int) tag);
  waittag (tag);

  sd.sd_ea = argp;		// save PPE address of sd block
  sd.value = sd.ad.sol;		// save PPE address of solution vector
  sd.size = ROUND_UP_128 (sd.ad.size_in_bytes);
  sd.ad.sol = memalign (16, sd.size); // allocate LS block
  if (sd.ad.sol == NULL) {
    fprintf (stderr,
	     "%s:%d: malloc failed in SPU %d\n", __FILE__, __LINE__, sd.num);
    exit(1);
  }


#if defined(DEBUG) && (DEBUG & 16)
  printf ("spe%d: &sd=0x%x, sd.value=0x%x, sd.ad.sol=0x%x\n",
	  sd.num, &sd, sd.value, sd.ad.sol);
#endif
  // -- *TBD* -- does sd.value need to be remapped (EA?)
  // -- get value vector from system memory into new LS block -----------------
  DPRINTF (" [%lld] mfc_get (0x%x, 0x%x, %d, %lu, 0, 0)\n", speid,
	   (unsigned) sd.ad.sol, (unsigned) sd.value,
	   sd.size, tag);
  

  // -- fix pb with DMA limitation (max = 16 KB) ------------------------------
  {
    int nbytes = sd.size;
    char *addr_ls = (char *) sd.ad.sol;
    char *addr_ea = (char *) sd.value;

    do {
      int sz = (nbytes < SPU_MAX_DMA)? nbytes: SPU_MAX_DMA;

      mfc_getb (addr_ls, (uint32_t) addr_ea, sz, tag, 0, 0);
      waittag (tag);

      nbytes -= sz;
      addr_ls += sz;
      addr_ea += sz;
    } while (nbytes > 0);
  }

#if defined(DEBUG) && (DEBUG & 8)
  printf (" [%lld] as_init dump:", speid);
  printf ("   sd.num = %d", sd.num);
  printf ("   sd.ctx = %d", (int) sd.ctx);
  printf ("   sd.thr = %d\n", (int) sd.thr);
#endif

#if defined(DEBUG) && (DEBUG & 2)
  if (sd.ad.do_not_init) {
    printf ("(SPU %d: received data (do_not_init=1):\n", sd.num);
    Ad_Display(sd.ad.sol, &sd.ad, NULL);
    printf(")\n");
  }
#endif
  
  Randomize_Seed (sd.ad.seed ^ sd.num);

  // -- call the benchmark-specific solver
  Solve (&sd.ad);
  
  // -- put the solution back on main memory for the PPE to read
  as_send ();

  //  printf ("SPU main returning\n");
  return 0;
}

void as_exit (int status)
{
  mfc_tag_release (tag);
  exit (status);
}

void as_receive ()
{
  // -- *TBD* --

}

void as_send ()
{
  void *t;

  // -- send value vector back to system memory -------------------------------

  // -- fix pb with DMA limitation (max = 16 KB) ------------------------------
  {
    int nbytes = sd.size;
    char *addr_ls = (char *) sd.ad.sol;
    char *addr_ea = (char *) sd.value;

    do {
      int sz = (nbytes < SPU_MAX_DMA)? nbytes: SPU_MAX_DMA;

      mfc_putb (addr_ls, (unsigned int) addr_ea, sz, tag, 0, 0);
      waittag (tag);

      nbytes -= sz;
      addr_ls += sz;
      addr_ea += sz;
    } while (nbytes);
  }


  t = sd.ad.sol;	        // restore PPE address for solution vector
  sd.ad.sol = sd.value;

  // -- send sd block back (inc. ad) ------------------------------------------
  mfc_putb ((void *) &sd, sd.sd_ea, sizeof (sd), (int) tag, 0, 0);
  waittag (tag);

  sd.ad.sol = t;		// put it back.
}


// -- is there something available in my mailbox? -----------------------------
int as_mbx_avail ()
{
  return (spu_stat_in_mbox () > 0);
}

// -- read whatever is sitting in my mailbox ----------------------------------
int as_mbx_read ()
{
  return spu_read_in_mbox ();
}

// -- send a value to the next SPU --------------------------------------------
int as_mbx_send_next (int v)
{
  if (sd.mbx_nxt_ea)
    return write_in_mbox (v, (uint32_t) sd.mbx_nxt_ea, tag);

  return -1;
}

// -- send a value to all the other SPUs --------------------------------------
void as_mbx_send_all (int v)
{
  int i;

  for (i=0; i<NUM_SPES; ++i) {
    if (sd.mbx_mfc_ea[i])
      write_in_mbox (v, (uint32_t) sd.mbx_mfc_ea[i], tag);
  }
}

// -- copy problem data from the previous SPU ---------------------------------
//
// Note that this assumes all SPUs have *identical* locations for all
// problem data and parameters.  FIXME: This ought to be checked at
// initialization time.
//
// The point is that, if one SPE knows that its neighbor ("previous"
// SPE) has a better cost, it will silently grab its value and
// parameter vectors from the neighbor...

void as_mbx_copy_prev ()
{
  uint32_t prev = (sd.num - 1) % sd.num_thr;

  mfc_getb (sd.ad.sol,		// store here (load address)
	    (uint32_t) sd.spe_ls_ea[prev] +
	      (int) &sd.ad.sol, // same address but in other SPU
	    ROUND_UP_16(sd.size), // number of bytes to copy
	    tag, 0, 0);
  waittag (tag);
}
