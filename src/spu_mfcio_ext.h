// -------------------------------------------------------------- 
// (C)Copyright 2007,                                         
// International Business Machines Corporation, 
// All Rights Reserved.
// -------------------------------------------------------------- 
 
#ifndef _spu_mfcio_ext_h_
#define _spu_mfcio_ext_h_

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

static uint32_t msg[4]__attribute__ ((aligned (16)));

// ==========================================================================
// Definitions
// ==========================================================================
#define SPU_IN_MBOX_OFFSET 			0x0C // offset of mailbox status register from control area base
#define SPU_IN_MBOX_OFFSET_SLOT 	0x3  // 16B alignment of mailbox status register = (SPU_MBOX_STAT_OFFSET&0xF)>>2
#define SPU_MBOX_STAT_OFFSET 		0x14 // offset of mailbox status register from control area base
#define SPU_MBOX_STAT_OFFSET_SLOT 	0x1  // 16B alignment of mailbox status register = (SPU_MBOX_STAT_OFFSET&0xF)>>2

#define SPU_SIG_NOTIFY_OFFSET		0x0C // offset of signal notify 1 or 2 registera from signal notify 1 or 2 areas base
#define SPU_SIG_NOTIFY_OFFSET_SLOT	0x3  // 16B alignment of signal notify 1 or 2 register = (SPU_SIG_NOTIFY_OFFSET&0xF)>>2

// ==========================================================================
// Functions definitions
// ==========================================================================

inline int status_mbox(uint64_t ea_mfc, uint32_t tag_id);
inline int status_in_mbox(uint64_t ea_mfc, uint32_t tag_id);
inline int status_out_mbox(uint64_t ea_mfc, uint32_t tag_id);
inline int status_outintr_mbox(uint64_t ea_mfc, uint32_t tag_id);

int write_in_mbox(uint32_t data, uint64_t ea_mfc, uint32_t tag_id);
int write_signal1(uint32_t data, uint64_t ea_mfc, uint32_t tag_id);
int write_signal2(uint32_t data, uint64_t ea_mfc, uint32_t tag_id);

// returns the value of mailbox status register of remote SPE
inline int status_mbox(uint64_t ea_mfc, uint32_t tag_id)
{
	uint32_t status[4], idx;	
	uint64_t ea_stat_mbox = ea_mfc + SPU_MBOX_STAT_OFFSET;

	//printf("<SPE: ea_mfc=0x%llx, ea_stat_mbox=0x%llx\n", ea_mfc, ea_stat_mbox );
	
	idx = SPU_MBOX_STAT_OFFSET_SLOT;
	
	mfc_get((void *)&status[idx], ea_stat_mbox, sizeof(uint32_t), tag_id, 0, 0);
	mfc_write_tag_mask(1<<tag_id);
	mfc_read_tag_status_any();

    //printf("<SPE: Status=0x%x: OutIntrCnt=0x%x, InCnt=0x%x, OutCnt=0x%x\n", status[idx], 
	//		(status[idx]&0xffff0000)>>16, (status[idx]&0x0000ff00)>>8, (status[idx]&0x000000ff) );
	//printf("<SPE: status_mbox=%d\n", status[idx] );
	
	return status[idx];
}

// returns the status (counter) of inbound_mailbox of remote SPE
inline int status_in_mbox(uint64_t ea_mfc, uint32_t tag_id)
{
	int status = status_mbox( ea_mfc, tag_id);
	
	status = (status&0x0000ff00)>>8;
			
	//printf("<SPE: status_in_mbox=%d\n", status );
	
	return status;
}

// returns the status (counter) of outbound_mailbox of remote SPE
inline int status_out_mbox(uint64_t ea_mfc, uint32_t tag_id)
{
	int status = status_mbox( ea_mfc, tag_id);

	status = (status&0x000000ff);
			
	//printf("<SPE: status_out_mbox=%d\n", status[idx] );
	
	return status;
}

// returns the status (counter) of inbound_interrupt_mailbox of remote SPE
inline int status_outintr_mbox(uint64_t ea_mfc, uint32_t tag_id)
{
	int status = status_mbox( ea_mfc, tag_id);

	status = (status&0xffff0000)>>16;
			
	//printf("<SPE: status_outintr_mbox=%d\n", status[idx] );
	
	return status;
}

// writing to a remote SPE’s inbound mailbox
inline int write_in_mbox(uint32_t data, uint64_t ea_mfc, uint32_t tag_id){

	int status;
	uint64_t ea_in_mbox = ea_mfc + SPU_IN_MBOX_OFFSET;	
	uint32_t mbx[4], idx;

	//printf("<SPE: write_in_mbox: starts\n" );
	
	while( (status= status_in_mbox(ea_mfc, tag_id))<1);
	
	//printf("<SPE: write_in_mbox: status=%d\n", status );
	
	// printf("<SPE: ea_mfc=0x%llx, ea_in_mbox=0x%llx\n", ea_mfc, ea_in_mbox );
	
	idx = SPU_IN_MBOX_OFFSET_SLOT;
	mbx[idx] = data;
	
	mfc_put((void *)&mbx[idx], ea_in_mbox, sizeof(uint32_t), tag_id, 0, 0);
	mfc_write_tag_mask(1<<tag_id);
	mfc_read_tag_status_any();

	//printf("<SPE: write_in_mbox: complete\n" );	
	
	return 1; // number of mailbox being written
}

// signal a remote SPE’s signal1 register
inline int write_signal1(uint32_t data, uint64_t ea_sig1, uint32_t tag_id)
{
	uint64_t ea_sig1_notify = ea_sig1 + SPU_SIG_NOTIFY_OFFSET;	
	uint32_t idx;

	//printf("<SPE: write_signal1: starts\n" );
	
	//printf("<SPE: write_signal1:  ea_mfc=0x%llx, ea_in_mbox=0x%llx\n", ea_sig1, ea_sig1_notify );
	
	idx = SPU_SIG_NOTIFY_OFFSET_SLOT;
	msg[idx] = data;
	
	mfc_sndsig( &msg[idx], ea_sig1_notify, tag_id, 0,0)	;
	mfc_write_tag_mask(1<<tag_id);
	mfc_read_tag_status_any();

	//printf("<SPE: write_in_mbox: complete\n" );	
	
	return 1; // number of mailbox being written
}

// signal a remote SPE’s signal1 register
inline int write_signal2(uint32_t data, uint64_t ea_sig2, uint32_t tag_id)
{
	uint64_t ea_sig2_notify = ea_sig2 + SPU_SIG_NOTIFY_OFFSET;	
	uint32_t idx;

	//printf("<SPE: write_signal2: starts\n" );
	
	//printf("<SPE: write_signal2:  ea_mfc=0x%llx, ea_in_mbox=0x%llx\n", ea_sig2, ea_sig2_notify );
	
	idx = SPU_SIG_NOTIFY_OFFSET_SLOT;
	msg[idx] = data;
	
	mfc_sndsig( &msg[idx], ea_sig2_notify, tag_id, 0,0)	;
	mfc_write_tag_mask(1<<tag_id);
	mfc_read_tag_status_any();

	//printf("<SPE: write_in_mbox: complete\n" );	
	
	return 1; // number of mailbox being written
}








#endif

