/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  main_MPI.c: MPI extension for main.c
 */

#include <stdlib.h>                       /* exit() */
#include <string.h>                       /* strchr() */

#include "main_MPI.h"
#include "ad_solver.h"                    /* my_num */
#include "ad_solver_MPI.h"                /* tegami lists */

void
AS_MPI_initialization( Main_MPIData * mpi_data_ptr )
{
  int i ;

  for( i=0 ; i<RESULTS_CHAR_MSG_SIZE ; ++i )
    mpi_data_ptr->results[i] = '\0' ;

  /* Init communication structures */
  list_allocated_msgs.next = NULL ;
  list_allocated_msgs.previous = NULL ;
  list_sent_msgs.next = NULL ;
  list_sent_msgs.previous = NULL ;
  list_recv_msgs.next = NULL ;
  list_recv_msgs.previous = NULL ;
#if defined DEBUG_QUEUE
  list_sent_msgs.text = (char*)malloc(QUEUE_NAME_MAX_LENGTH*sizeof(char)) ;
  snprintf(list_sent_msgs.text,QUEUE_NAME_MAX_LENGTH,"Sent msgs") ;
  list_sent_msgs.size = 0 ;
  list_sent_msgs.nb_max_msgs_used = 0 ;
  list_recv_msgs.text = (char*)malloc(QUEUE_NAME_MAX_LENGTH*sizeof(char)) ;
  snprintf(list_recv_msgs.text,QUEUE_NAME_MAX_LENGTH,"Recv msgs") ;
  list_recv_msgs.size = 0 ;
  list_recv_msgs.nb_max_msgs_used = 0 ;
  list_allocated_msgs.text = (char*)malloc(QUEUE_NAME_MAX_LENGTH*sizeof(char));
  snprintf(list_allocated_msgs.text,QUEUE_NAME_MAX_LENGTH,"Allocated msgs") ;
  list_allocated_msgs.size = 0 ;
  list_allocated_msgs.nb_max_msgs_used = 0 ;
#endif /* DEBUG_QUEUE */

  /**************************** Initialize seed phase 1 **********************/
  if ((mpi_data_ptr->p_ad)->seed < 0) {
    srandom( (unsigned int)(mpi_data_ptr->tv_sec) );
      

    /* INT_MAX / 6 (= 357.913.941) is a reasonable value to start with... 
       I think... */
    *(mpi_data_ptr->last_value_ptr) = (int) Random(INT_MAX / 6);
      
    /* INT_MAX (= 2.147.483.647) is the max value for a signed 32-bit int */
    (mpi_data_ptr->p_ad)->seed = randChaos(INT_MAX,
					   mpi_data_ptr->last_value_ptr,
					   mpi_data_ptr->param_a_ptr,
					   mpi_data_ptr->param_c_ptr);
  }
  *(mpi_data_ptr->print_seed_ptr) = (mpi_data_ptr->p_ad)->seed;

  for(i = 0; i < my_num; ++i)
    (mpi_data_ptr->p_ad)->seed = randChaos(INT_MAX,
					   mpi_data_ptr->last_value_ptr,
					   mpi_data_ptr->param_a_ptr,
					   mpi_data_ptr->param_c_ptr);

  /* forced count to 1 (-b 1) */
  *(mpi_data_ptr->count_ptr) = 1 ;

  DPRINTF("Proc %d computed seed %d\n\n", my_num, (mpi_data_ptr->p_ad)->seed);
  PRINT0("Number of procs used: %d\n", mpi_size) ;

#if defined PRINT_COSTS
  i=mpi_size ;
  *(mpi_data_ptr->nb_digits_nbprocs_ptr)=0 ;
  do {
    i=i/10 ;
    ++*(mpi_data_ptr->nb_digits_nbprocs_ptr) ;
  } while( i!=0 ) ;
#endif

}

void
AS_MPI_initialization_epilogue( Main_MPIData * mpi_data_ptr )
{
  int i ;
  int nb_stocked_messages ;
  tegami * tmp_tegami_ptr ;

#if defined COMM_CONFIG
  mpi_data_ptr->size_message = 
    mpi_data_ptr->p_ad->size + 5 ; 
#elif defined SIZE_MESSAGE
  mpi_data_ptr->size_message = SIZE_MESSAGE ;
#else
  printf("SIZE_MESSAGE undefined, and COMM_CONFIG not set. Aborting!\n") ;
  exit(-1) ;
#endif
  DPRINTF("- Size of a message is %d\n", mpi_data_ptr->size_message) ;

  /**************************** Init messages *******************************/
  if( mpi_size <= 2 )
    nb_stocked_messages = 100 ;
  else
    nb_stocked_messages = 4*(mpi_size)*(mpi_size) ;

#if defined COMM_CONFIG
  /* Manage dynamic memory */
  mpi_data_ptr->block_of_messages = (unsigned int *)
    malloc( nb_stocked_messages * 
	    mpi_data_ptr->size_message * sizeof(unsigned int) ) ;
  if( mpi_data_ptr->block_of_messages == NULL ) {
    printf("Error memory allocation\n") ;
    exit(0) ;
  }    
#endif

  for( i=0 ; i<nb_stocked_messages ; ++i ) {
    tmp_tegami_ptr = (tegami*)malloc(sizeof(tegami)) ;
#if defined COMM_CONFIG
    /*    tmp_tegami_ptr->message = mpi_data_ptr->block_of_messages + 
	  (i * mpi_data_ptr->size_message ) ; */
    tmp_tegami_ptr->message = (unsigned int *)
      malloc( mpi_data_ptr->size_message * sizeof(unsigned int) ) ;
#endif
    push_tegami_on( tmp_tegami_ptr,
		    &list_allocated_msgs) ;
  }
  PRINT0("Prepared %d messages!\n", nb_stocked_messages) ;

  /*************************** Launch async recv: will act as mailbox ********/
  Gthe_message = get_tegami_from( &list_allocated_msgs ) ;
  TDPRINTF("%d launches MPI_Irecv(), any source\n", my_num) ;
  MPI_Irecv(Gthe_message->message, mpi_data_ptr->size_message, MPI_INT,
	    MPI_ANY_SOURCE, 
	    MPI_ANY_TAG,
	    MPI_COMM_WORLD, &(Gthe_message->handle)) ;
}

void
AS_MPI_completion( Main_MPIData * mpi_data_ptr )
{
  int flag ;
  tegami * tmp_message ;
  char recv_results[RESULTS_CHAR_MSG_SIZE] ;  /* To recv perf, if p0 finishes */

  /* From now, time is not crucial anymore... */
  /* Perform a broadcast to kill everyone since I have the solution */
  
  DPRINTF("Proc %d enters finishing state!\n", my_num) ;
#if defined STATS
  TPRINTF("======== STATS MPI =========\n") ;
  TPRINTF("Nb sent messages: %d", Gmpi_stats.nb_sent_messages) ;
  TPRINTF("Nb my messages: %d", Gmpi_stats.nb_sent_mymessages) ;
  TPRINTF("======== END STATS MPI =========\n") ;
#endif
#if defined MPI_ABORT
  PRINTF("%s\n", mpi_data_ptr->results) ;
  TPRINTF("%d launches MPI_Abort()!\n", my_num) ;
  MPI_Abort(MPI_COMM_WORLD, my_num) ;
  exit(0) ;
#endif

  if( my_num != 0 ) { 
    /*********************************** Proc N ! ****************************/
    /****** Use *Log(n) + 1*  algo */
    /*** Send LS_KILLALL to 0 -> no real message => no need to init */
    /* But proc 0 can also be in that step, so Isend() mandatory! */
    tmp_message = get_tegami_from( &list_allocated_msgs ) ;
    /*    tmp_message->message[1] = my_num ; */
    TDPRINTF("%d launches MPI_Isend(), LS_KILLALL to 0\n", my_num) ;
    MPI_Isend(tmp_message->message, mpi_data_ptr->size_message, MPI_INT,
	      0,
	      LS_KILLALL, MPI_COMM_WORLD,
	      &(tmp_message->handle)) ;
    push_tegami_on( tmp_message, &list_sent_msgs ) ;
    /* Loop on all received msg. Drop all except LS_KILLALL */
    TDPRINTF("%d loops on recvd msgs\n", my_num) ;
    do {
      TDPRINTF("%d launches MPI_Wait()\n", my_num) ;
      MPI_Wait(&(Gthe_message->handle), &(Gthe_message->status)) ;
#if !defined COMM_CONFIG
      TDPRINTF("%d recvd value (%d;%d) protocol %s from %d\n", my_num,
	       Gthe_message->message[0],
	       Gthe_message->message[1],
	       protocole_name[Gthe_message->status.MPI_TAG],
	       Gthe_message->status.MPI_SOURCE) ;
#else
      TDPRINTF("%d recvd msg with protocol %s from %d\n",
	       my_num,
	       protocole_name[Gthe_message->status.MPI_TAG],
	       Gthe_message->status.MPI_SOURCE) ;
#endif
      if( Gthe_message->status.MPI_TAG != LS_KILLALL ) {
	/* Launch new rcv for input comm */
	TDPRINTF("%d launches MPI_Irecv(), ANY_TAG, any source\n", my_num) ;
	MPI_Irecv(Gthe_message->message, mpi_data_ptr->size_message, MPI_INT,
		  MPI_ANY_SOURCE, 
		  MPI_ANY_TAG,
		  MPI_COMM_WORLD, &(Gthe_message->handle)) ;
      } 
    } while( Gthe_message->status.MPI_TAG != LS_KILLALL ) ;
    DPRINTF("%d received msg from %d that %d finished.\n\n",
	    my_num, Gthe_message->status.MPI_SOURCE, Gthe_message->message[1]) ;
    /* Kill sub-range proc */
    send_log_n(mpi_data_ptr->size_message, Gthe_message->message, LS_KILLALL) ;
    /* Management of results! */
    if( Gthe_message->message[1] == (unsigned)my_num ) { /* I'm the winner */
      /* Send results to 0 */
      TDPRINTF("%d launches MPI_Isend(), results to 0\n", my_num) ;
      MPI_Send( mpi_data_ptr->results, RESULTS_CHAR_MSG_SIZE, MPI_CHAR,
		0, SENDING_RESULTS,
		MPI_COMM_WORLD) ;
    } /* if( the_message->message[1] == (unsigned)my_num ) */
    /* Finishing for all processes (!=0) */
#if defined PRINT_COSTS
    print_costs() ;
#endif
    TDPRINTF("%d calls MPI_Finalize()\n", my_num) ;
    MPI_Finalize() ;
    dead_end_final() ;
    exit(0) ;
  } else { /************************** Proc 0 ! ****************************/
    /* Check if we received a LS_KILLALL before we finished the calculus */
    DPRINTF("%d checks if we received LS_KILLALL in last msgs...\n", my_num) ;
    do {
      TDPRINTF("%d launches MPI_Test()\n", my_num) ;
      MPI_Test(&(Gthe_message->handle),
	       &flag,
	       &(Gthe_message->status)) ;
      if( flag > 0 ) {                /* We received one msg! */
	TDPRINTF("%d received message %d protocol %s from %d\n", my_num,
		 Gthe_message->message[1],
		 protocole_name[Gthe_message->status.MPI_TAG],
		 Gthe_message->status.MPI_SOURCE) ;
	if( Gthe_message->status.MPI_TAG == LS_KILLALL ) {
	  /* The first proc having sent this msg is the winner */
	  Gthe_message->message[0] = mpi_size ;
	  Gthe_message->message[1] = Gthe_message->status.MPI_SOURCE ;
	  send_log_n( mpi_data_ptr->size_message,
		      Gthe_message->message, LS_KILLALL ) ;
	  /* Now, recv result from the first having sent its results: winner */
	  TDPRINTF("%d launches MPI_Irecv() of results for source %d\n",
		   my_num,
		   Gthe_message->status.MPI_SOURCE) ;
	  MPI_Recv( recv_results, RESULTS_CHAR_MSG_SIZE, MPI_CHAR,
		    Gthe_message->status.MPI_SOURCE, SENDING_RESULTS,
		    MPI_COMM_WORLD,
		    MPI_STATUS_IGNORE) ;
	  /**** Compare its result to our! */
	  DPRINTF("Recvd : %s\n", recv_results) ;
	  DPRINTF("  -> %s\n",
		  strchr(strchr(recv_results+1,'|')+1, '|')+1) ;
	  DPRINTF("Computed : %s\n", mpi_data_ptr->results) ;
	  DPRINTF("  ->%s\n",
		  strchr(strchr(mpi_data_ptr->results+1,'|')+1, '|')+1) ;
	  /* Search for 3rd | in string */
	  if( atof(strchr(strchr(&recv_results[1],'|')+1, '|')+1)
	      > atof(strchr(strchr(&(mpi_data_ptr->results[1]),'|')+1, '|')+1) )
	    PRINTF("%s\n\n", mpi_data_ptr->results ) ;
	  else
	    PRINTF("%s\n\n", recv_results ) ;
#if defined PRINT_COSTS
	  print_costs() ;
#endif
	  TPRINTF("0 calls MPI_Finalize()\n") ;
	  MPI_Finalize() ;
	  /* Do we have to recv the other sent msgs to make a good terminating
	     process? */
	  dead_end_final() ;
	  exit(0) ;
	} else { /* Status != LSKILLALL, then prepare test for new msg */
	  MPI_Irecv(Gthe_message->message, mpi_data_ptr->size_message,
		    MPI_INT,
		    MPI_ANY_SOURCE, 
		    MPI_ANY_TAG,
		    MPI_COMM_WORLD, &(Gthe_message->handle)) ;
	}
      }
    } while( flag > 0 ) ; /* exit if no msg rcvd */
    /* From here, proc 0 winner */
    PRINTF("%s\n\n", mpi_data_ptr->results) ;
#if defined PRINT_COSTS
    print_costs() ;
#endif
    /* Cancel Irecv */
    TDPRINTF("%d launches MPI_Cancel()\n", my_num) ;
    MPI_Cancel( &(Gthe_message->handle) ) ;
    MPI_Wait( &(Gthe_message->handle), MPI_STATUS_IGNORE ) ;
    TDPRINTF("%d finished Waiting of canceled msg\n", my_num) ;

    /* Reuse buffer and send LSKILLALL to everyone */
    Gthe_message->message[1] = 0 ;
    Gthe_message->message[0] = mpi_size ;
    send_log_n( mpi_data_ptr->size_message,
		Gthe_message->message,
		LS_KILLALL ) ;
    TDPRINTF("0 calls MPI_Finalize()\n") ;
    MPI_Finalize() ;
    dead_end_final() ;
    exit(0) ;
  } /* Proc N // Proc 0 */
}
