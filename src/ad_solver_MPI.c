/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  ad_solver_MPI.c: MPI extension of ad_solver
 */

#include "ad_solver_MPI.h"
#include "tools_MPI.h"
#include <math.h>               /* ceil() */
#include <stdlib.h>             /* free() */
#include <unistd.h>             /* sleep() */
#include <time.h>               /* time() */
#include <sys/time.h>           /* gettimeofday() */
#include <string.h>             /* memcpy() */

/*----------------------*
 * Constants and macros
 *----------------------*/

/*-------*
 * Types *
 *-------*/

char * protocole_name [LS_NBMSGS] =
  {
    "Not a protocol",
    "Killall",
    "Sending results",
    "Sending cost",
    "Sending iter",
    "Sending (cost and iter)",
    "Sending (config, cost+iter+stats)"
  } ;

/*------------------*
 * Global variables *
 *------------------*/

extern int vec_costs[500000] ;
extern unsigned long int card_vec_costs ;

/*------------*
 * Prototypes *
 *------------*/

#if defined PRINT_COSTS
void print_costs()
{
  unsigned long int i ;
  
  char line[200] ;
  
  sprintf(line,"*** Costs for %d\n", my_num);
  writen(file_descriptor_print_cost, line, strlen(line)) ;

  for( i=0 ; i<=card_vec_costs ; i++ ) {
    sprintf(line,"%ld    %d   %d\n", i, vec_costs[i], my_num) ;
    writen(file_descriptor_print_cost, line, strlen(line)) ;
  }
  sprintf(line,"*** Fin Costs for %d\n", my_num);
  writen(file_descriptor_print_cost, line, strlen(line)) ;
}
#endif /* defined PRINT_COSTS */

void
Ad_Solve_init_MPI_data( Ad_Solve_MPIData * mpi_data_ptr )
{
  mpi_data_ptr->nb_block = 1 ;

#if (defined COMM_COST)
  mpi_data_ptr->best_cost_sent = INT_MAX ;
  mpi_data_ptr->total_min_cost_received = INT_MAX ;
  s_cost_message = (unsigned int *)
    malloc(mpi_data_ptr->p_ad->main_mpi_data_ptr->size_message * sizeof(int)) ;
#endif /* COMM_COST */

#if defined ITER_COST
  mpi_data_ptr->best_cost_sent = INT_MAX ;
  mpi_data_ptr->total_min_cost_received = INT_MAX ;
  s_cost_message = (unsigned int *)
    malloc(mpi_data_ptr->p_ad->main_mpi_data_ptr->size_message * sizeof(int)) ;

  mpi_data_ptr->iter_of_best_cost_sent = INT_MAX ;
  mpi_data_ptr->mixed_received_msg_iter = -1 ;
#endif /* ITER_COST */

#if defined COMM_CONFIG
  mpi_data_ptr->best_cost_sent = INT_MAX ;
  mpi_data_ptr->total_min_cost_received = INT_MAX ;
  mpi_data_ptr->s_cost_message = (unsigned int *)
    malloc(((mpi_data_ptr->p_ad)->main_mpi_data_ptr)->size_message * sizeof(int)) ;

  mpi_data_ptr->iter_of_best_cost_sent = INT_MAX ;
  mpi_data_ptr->iter_for_best_cost_received = -1 ;

  /* Could be a ptr on s_cost_message... */
  mpi_data_ptr->cpy_best_msg = (unsigned int *)
    malloc(mpi_data_ptr->p_ad->main_mpi_data_ptr->size_message * sizeof(int)) ;
#endif /* COMM_CONFIG */

#if defined MIN_ITER_BEFORE_RESTART
  mpi_data_ptr->nbiter_since_restart = 0 ;
#endif
}

/* Try to free old messages in their sending order */
void
Ad_Solver_free_messages( Ad_Solve_MPIData * mpi_data_ptr )
{
  int flag = 1 ;       /* .. if operation completed */
  AdData * p_ad = mpi_data_ptr->p_ad ;

  TDPRINTF("--------------- proc %d, CBlock %d, iter %d:"
	   " Free older messages\n", 
	   my_num,
	   mpi_data_ptr->nb_block,
	   p_ad->nb_iter
	   ) ;
  while( (list_sent_msgs.next != NULL) && (flag!=0) ) {
    TDPRINTF(" Proc %d launches MPI_TEST()\n", my_num) ;
    MPI_Test( &((list_sent_msgs.previous)->handle), &flag,
	      MPI_STATUS_IGNORE ) ;
    if( flag != 0 )
      push_tegami_on( unqueue_tegami_from(&list_sent_msgs),
		      &list_allocated_msgs) ;
  } /* while( (list_sent_msgs.next != NULL) && (flag!=0) ) { */
}

/* Reception of all messages */
int
Ad_Solver_recv_messages( Ad_Solve_MPIData * mpi_data_ptr )
{
  int flag = 0 ;       /* .. if operation completed */
  char results[RESULTS_CHAR_MSG_SIZE] ;
  int number_received_msgs ;
  Main_MPIData * main_mpi_data_ptr = mpi_data_ptr->p_ad->main_mpi_data_ptr ;

  number_received_msgs = 0 ;

  DPRINTF("-------------- %d: Reception\n", my_num) ;
  /************** Check how many msg were received ***************/
  do {
    TDPRINTF("Proc %d launches MPI_TEST()\n", my_num) ;
    MPI_Test(&(Gthe_message->handle), &flag, &(Gthe_message->status)) ;
    if( flag > 0 ) {                     /* We received at least one! */
      TDPRINTF("%d received protocol %s from %d\n",
	      my_num,
	      protocole_name[Gthe_message->status.MPI_TAG],
	      Gthe_message->status.MPI_SOURCE) ;
      /* Initiate treatment of all msg */
      number_received_msgs++ ;
      switch( Gthe_message->status.MPI_TAG ) {
	/******************************* LS_KILLALL *********************/
      case LS_KILLALL:
	/* Then quits! */
	DPRINTF("LS_KILLALL from %d\n",
		Gthe_message->status.MPI_SOURCE) ;
	if( my_num == 0 ) {
	  /* Reuse msg to send LSKILLALL to everyone */
	  Gthe_message->message[0] = mpi_size ;
	  Gthe_message->message[1] = Gthe_message->status.MPI_SOURCE ;
	  send_log_n( main_mpi_data_ptr->size_message,
		      Gthe_message->message, LS_KILLALL ) ;
	  /* Recv results from source */
	  TDPRINTF("%d launches MPI_Irecv() of results for source %d\n",
		  my_num, Gthe_message->status.MPI_SOURCE) ;
	  MPI_Recv( results, RESULTS_CHAR_MSG_SIZE, MPI_CHAR,
		    Gthe_message->status.MPI_SOURCE, SENDING_RESULTS,
		    MPI_COMM_WORLD,
		    MPI_STATUS_IGNORE) ;
	  PRINTF("%s\n", results) ;
#if defined PRINT_COSTS
	  print_costs() ;
#endif
	  TPRINTF("0 calls MPI_Finalize()\n") ;
	  MPI_Finalize() ;
	  dead_end_final() ;
	  exit(0) ;
	} else {                                  /* Proc N */
	  /* S.o finished before me. I killall the ones I'm responsible */
	  send_log_n( main_mpi_data_ptr->size_message,
		      Gthe_message->message, LS_KILLALL ) ;
#if defined PRINT_COSTS
	  print_costs() ;
#endif
	  TPRINTF("%d calls MPI_Finalize()\n", my_num) ;
	  MPI_Finalize() ;
	  dead_end_final() ;
	  exit(0) ;
	}
	break ;
	/******************************* LS_COST *********************/
#if (defined COMM_COST) || (defined ITER_COST)
      case LS_COST:     /* take the min! */
	DPRINTF("%d takes the min between min'_recv(%d) and recvd(%d)\n",
		my_num,
		mpi_data_ptr->min_cost_received, Gthe_message->message[1]) ;
	if( mpi_data_ptr->min_cost_received >
	    Gthe_message->message[1] ) {
	  mpi_data_ptr->min_cost_received = Gthe_message->message[1] ;
#if defined ITER_COST
	  /* Save according iter */
	  mpi_data_ptr->min_received_msg_iter = Gthe_message->message[2] ;
#endif
	}
	/* Store recvd msg to treat it later */
	push_tegami_on( Gthe_message, &list_recv_msgs) ;
	/* Launch new async recv */
	Gthe_message = get_tegami_from( &list_allocated_msgs) ;
	TDPRINTF("%d launches MPI_Irecv(), any source\n", my_num) ;
	MPI_Irecv(Gthe_message->message, main_mpi_data_ptr->size_message,
		  MPI_INT,
		  MPI_ANY_SOURCE, 
		  MPI_ANY_TAG,
		  MPI_COMM_WORLD, &(the_message->handle)) ;
	break ;
#endif /* COMM_COST || ITER_COST */
	/******************************* LS_CONFIG *********************/
#if defined COMM_CONFIG
      case LS_CONFIG:
	DPRINTF("%d takes the min between min'_recv(%d) and recvd(%d)\n",
		my_num,
		mpi_data_ptr->min_cost_received,
		Gthe_message->message[ main_mpi_data_ptr->size_message - 4 ]) ;

	if( mpi_data_ptr->min_cost_received >
	    Gthe_message->message[ main_mpi_data_ptr->size_message - 4 ] ) {
	  /* Save according iter */
	  mpi_data_ptr->min_cost_received =
	    Gthe_message->message[ main_mpi_data_ptr->size_message - 4 ] ;
	  /* Save according iter */
	  mpi_data_ptr->min_received_msg_iter =
	    Gthe_message->message[ main_mpi_data_ptr->size_message - 3 ] ;
	  /* Save message address */
	  mpi_data_ptr->tmp_best_msg_ptr = Gthe_message->message ;
	}
	/* Store recvd msg to treat it later */
	push_tegami_on( Gthe_message, &list_recv_msgs) ;
	/* Launch new async recv */
	Gthe_message = get_tegami_from( &list_allocated_msgs ) ;
	TDPRINTF("%d launches MPI_Irecv(), any source\n", my_num) ;
	MPI_Irecv(Gthe_message->message, main_mpi_data_ptr->size_message,
		  MPI_INT,
		  MPI_ANY_SOURCE, 
		  MPI_ANY_TAG,
		  MPI_COMM_WORLD, &(Gthe_message->handle)) ;
	break;
#endif /* COMM_CONFIG */
      case SENDING_RESULTS:
      default:
	printf("Should never happen! Exiting.\n") ;
	exit(-1) ;
      } /*  switch( the_message->status.MPI_TAG ) { */
    } /* if flag */
  } while( flag > 0 ) ;

#if defined COMM_CONFIG
  /* Deep copy of the best configuration received */
  if( mpi_data_ptr->tmp_best_msg_ptr != NULL )
    memcpy(mpi_data_ptr->cpy_best_msg, mpi_data_ptr->tmp_best_msg_ptr,
	   main_mpi_data_ptr->size_message) ;
#endif

  return( number_received_msgs ) ;
}

/** If exit with 10, restart! */
int
Ad_Solve_manage_MPI_communications( Ad_Solve_MPIData * mpi_data_ptr )
{
  int i ;
  int number_received_msgs ;  /* # of received msgs in a block of iterations */
  tegami * tmp_tegami ;
#if (defined COMM_COST) || (defined ITER_COST) || (defined COMM_CONFIG)
  unsigned int ran_tmp ;
#endif /* COMM_COST || ITER_COST || COMM_CONFIG */
  AdData * p_ad = mpi_data_ptr->p_ad ;
  Main_MPIData * main_mpi_data_ptr = mpi_data_ptr->p_ad->main_mpi_data_ptr ;
#if (defined COMM_CONFIG)
  int take_sol=0 ; /* to know if we changed our sol. Then donc send! */
#endif

  /*  if( ((p_ad->nb_iter_tot+p_ad->nb_iter) % count_to_communication)==0 ) {*/
  if( (p_ad->nb_iter_tot+p_ad->nb_iter) > mpi_data_ptr->nb_block*count_to_communication ) {
    mpi_data_ptr->nb_block++ ;
    /* Reinit of some values */
#if (defined COMM_COST) || (defined ITER_COST)
    mpi_data_ptr->min_cost_received = INT_MAX ;
#endif
#if defined COMM_CONFIG
    mpi_data_ptr->tmp_best_msg_ptr = NULL ;
    mpi_data_ptr->min_cost_received = INT_MAX ;
#endif
    /* Try to free some previous messages */
    Ad_Solver_free_messages( mpi_data_ptr ) ;
    /* Check if recvd msgs */
    number_received_msgs = Ad_Solver_recv_messages( mpi_data_ptr ) ;
    DPRINTF("%d received %d messages in total this time\n",
	    my_num,
	    number_received_msgs) ;
    DPRINTF("-------------- %d: Treatment\n", my_num) ;
    /****************** Treat all received msgs (except LS_KILLALL) **/
    for( i=0 ; i<number_received_msgs ; i++ ) {
      tmp_tegami = get_tegami_from( &list_recv_msgs ) ;
      switch( tmp_tegami->status.MPI_TAG ) {
	/******************************* LS_COST *********************/
	/* For COST, ITER and ITER_COST */
#if (defined COMM_COST) || (defined ITER_COST)
      case LS_COST:
	TDPRINTF("%d treats LS_COST of %d from %d\n",
		 my_num,
		 tmp_tegami->message[1],
		 tmp_tegami->status.MPI_SOURCE) ;
	/* Crash cost with our value and avoids a test */
	tmp_tegami->message[1] = mpi_data_ptr->min_cost_received ;
#if defined ITER_COST
	tmp_tegami->message[2] = mpi_data_ptr->min_received_msg_iter ;
#endif
	/* [cont. to] Distribute the information */
	send_log_n( main_mpi_data_ptr->size_message, tmp_tegami->message,
		    LS_COST ) ;
	/* Msg received and treated */
	push_tegami_on( tmp_tegami, &list_allocated_msgs) ;
	break;
#endif /* COMM_COST || ITER_COST */
	/**************************** LS_CONFIG *********************/
#if defined COMM_CONFIG
      case LS_CONFIG:
	TDPRINTF("%d treats LS_CONFIG of cost %d iter %d from %d\n",
		 my_num,
		 tmp_tegami->message[ main_mpi_data_ptr->size_message - 4 ],
		 tmp_tegami->message[ main_mpi_data_ptr->size_message - 3 ],
		 tmp_tegami->status.MPI_SOURCE) ;
	if( (mpi_data_ptr->tmp_best_msg_ptr != tmp_tegami->message) ) {
	  /* &&
	     (tmp_tegami->message[0] > 0) ) { */
	  /* Crush messages, but not the range! */
	  TDPRINTF("%d crushes msg %d on %d with config with best cost\n",
		   my_num,
		   i,
		   number_received_msgs) ;
	  memcpy(tmp_tegami->message + 1, mpi_data_ptr->cpy_best_msg + 1,
		 main_mpi_data_ptr->size_message - 1) ;
	}
	send_log_n( main_mpi_data_ptr->size_message,
		    tmp_tegami->message, LS_CONFIG ) ;
	/* Msg received and treated */
	push_tegami_on( tmp_tegami, &list_allocated_msgs) ;
	break ;
#endif /* COMM_CONFIG */
      default:
	PRINTF("This should never happen! Exiting...\n") ;
	exit(-1) ;
      } /* switch( the_message->status.MPI_TAG ) { */
    } /* for( i=0 ; i<number_received_msgs ; i++ ) { */
#if defined DEBUG
    DPRINTF("-------------- %d: Impact\n", my_num) ;
#endif
    /*------------------------------------------------------------*/
    /*                  Repercusion of messages on me             */
#if (defined COMM_COST) || (defined ITER_COST) || (defined COMM_CONFIG)
    if( number_received_msgs > 0 ) {
      /** Update total values */
      if( mpi_data_ptr->min_cost_received
	  < mpi_data_ptr->total_min_cost_received ) {
	mpi_data_ptr->total_min_cost_received =
	  mpi_data_ptr->min_cost_received ;
#if (defined ITER_COST) || (defined COMM_CONFIG)
	mpi_data_ptr->iter_for_best_cost_received =
	  mpi_data_ptr->min_received_msg_iter ;
#endif
      }
      DPRINTF("%d: Best ever recv cost is now %d\n",
	      my_num, mpi_data_ptr->total_min_cost_received) ;
#if (defined ITER_COST) || (defined COMM_CONFIG)
      DPRINTF("... with #iter %d\n",
	      mpi_data_ptr->iter_for_best_cost_received ) ;
#endif
      /******************************* COMM_COST ****************/
#if defined COMM_COST
      if( (unsigned)p_ad->total_cost > mpi_data_ptr->min_cost_received ) {
	ran_tmp=(((float)random())/RAND_MAX)*100 ;
	DPRINTF("Proc %d (cost %d > %d cost received):"
		" ran=%d >?< proba user %d\n",
		my_num,
		p_ad->total_cost,
		mpi_data_ptr->min_cost_received,
		ran_tmp,
		proba_communication) ;
	if( ran_tmp < proba_communication ) {
#if defined DEBUG_RESTART
	  TDPRINTF("Proc %d restarts!\n", my_num) ;
#endif
	  return 10 ;
	} /* if( tan_tmp */  
      } /* if( p_ad->total.cost */
#endif /* COMM_COST */
      /******************************* ITER_COST ****************/
#if defined ITER_COST
      /* Best cost AND smaller #iter */
      if( (unsigned)p_ad->total_cost > mpi_data-ptr->min_cost_received ) {
	if( mpi_data_ptr->min_received_msg_iter < p_ad->nb_iter ) {
	  
	  ran_tmp=(((float)random())/RAND_MAX)*100 ;
#if defined DEBUG	  
	  DPRINTF("Proc %d (cost %d > %d): ran=%d >?< %d\n",
		  my_num,
		  p_ad->total_cost,
		  mpi_data_ptr->min_cost_received,
		  ran_tmp,
		  proba_communication) ;
#endif
	  if( ran_tmp < proba_communication ) {
#if defined DEBUG_RESTART
	    TDPRINTF("Proc %d restarts!\n", my_num) ;
#endif
	    return 10 ;
	  } /* if( tan_tmp */  
	}/* if( min_recv */
      } /* if( p_ad->total.cost */
#endif /* ITER_COST */
      /******************************* COMM_CONFIG ****************/
#if defined COMM_CONFIG
      /* Best cost */
      if( (unsigned int)p_ad->total_cost > cost_threshold ) {
	if( (unsigned)p_ad->total_cost > mpi_data_ptr->min_cost_received ) {
	  /*	ran_tmp=(((float)random())/RAND_MAX)*100 ; */
	  ran_tmp = 0 ;
#if defined DEBUG	  
	  DPRINTF("Proc %d (current cost %d > %d cost received):"
		  " ran=%d >?< %d proba user\n",
		  my_num,
		  p_ad->total_cost,
		  mpi_data_ptr->min_cost_received,
		  ran_tmp,
		  proba_communication) ;
	  if( mpi_data_ptr->tmp_best_msg_ptr == NULL ) {
	    printf("Pointer on best solution received is NULL and"
		   " our total cost is less than the cost received?\n"
		   "Exiting...\n\n") ;
	    exit(-1) ;
	  }
#endif
	  if( ran_tmp <= proba_communication ) {
	    TDPRINTF("Proc %d crushes its config with"
		     " received configuration!\n", my_num) ;
	    take_sol = 1 ;
	    /* Crush configuration and update information */
	    memcpy( p_ad->sol,
		    mpi_data_ptr->cpy_best_msg + 1,
		    main_mpi_data_ptr->size_message - 5 ) ;
	    p_ad->total_cost =  /* also = mpi_data_ptr->min_cost_received ; */
	      mpi_data_ptr->cpy_best_msg[ main_mpi_data_ptr->size_message - 4 ] ;
	    p_ad->nb_iter_tot += p_ad->nb_iter -
	      mpi_data_ptr->cpy_best_msg[ main_mpi_data_ptr->size_message - 3 ] ;
	    p_ad->nb_iter = 
	      mpi_data_ptr->cpy_best_msg[ main_mpi_data_ptr->size_message - 3 ] ;
	    TDPRINTF("TODO: compteurs cumules") ; /* nb swap ? */
	  } /* if( tan_tmp */  
	} /* if( p_ad->total.cost */
      } 
#if defined DEBUG
      else {      /* if( p_ad->total.cost */
	TDPRINTF("Don't do anything since current cost %d < %d user value\n",
		 (unsigned int)p_ad->total_cost,
		 cost_threshold ) ;
      }
#endif
#endif /* COMM_CONFIG */
    } /* if( number_received_msgs > 0 ) { */
#endif /* COMM_COST || ITER_COST || COMM_CONFIG */

    /*-------------------------------------------------------------*/
    /**************************** Sending **************************/
    /*-------------------------------------------------------------*/
#if defined DEBUG
    DPRINTF("-------------- %d: Sending\n", my_num) ;
#endif	
#if (defined COMM_COST) || (defined ITER_COST)
    /************** Sends best cost? ************/
    if( (unsigned)best_cost < mpi_data->best_cost_sent ) {
#if defined DEBUG
      DPRINTF("Proc %d sends cost %d\n", my_num, best_cost) ;
#endif /* DEBUG */
      mpi_data->s_cost_message[0] = mpi_size ;
      mpi_data->s_cost_message[1] = best_cost ;
#if defined ITER_COST
	mpi_data->s_cost_message[2] = p_ad->nb_iter ;
#endif
      send_log_n(mpi_data->s_cost_message, LS_COST) ;	      
      mpi_data->best_cost_sent = best_cost ;
#if defined ITER_COST
      mpi_data->iter_of_best_cost_sent = p_ad->nb_iter ;
#endif
    } /* if( best_cost < best_cost_sent ) { */
#endif /* COMM_COST || ITER_COST */

#if (defined COMM_CONFIG)
    /************** Send? ************/
      /* Compare to the last cost received since everyone can have
	 done a restart */
    TDPRINTF(": %d sends if total_cost %d < min_rcvd %d...\n",
	     my_num,
	     p_ad->total_cost,
	     mpi_data_ptr->min_cost_received) ;
    if( take_sol == 1 )
      TDPRINTF("... but %d crushed his sol so don't send!\n",
	       my_num) ;
    else {	       
      if( p_ad->total_cost < mpi_data_ptr->min_cost_received ) {
	if( mpi_data_ptr->best_cost_sent > p_ad->total_cost ) {
	  mpi_data_ptr->best_cost_sent = p_ad->total_cost ;
	  mpi_data_ptr->iter_of_best_cost_sent = p_ad->nb_iter ;
	}
	DPRINTF("Proc %d sends cost %d\n", my_num, p_ad->total_cost) ;
	mpi_data_ptr->s_cost_message[0] = mpi_size ;
	memcpy( mpi_data_ptr->s_cost_message + 1,
		p_ad->sol,
		main_mpi_data_ptr->size_message - 5) ;
	mpi_data_ptr->s_cost_message[main_mpi_data_ptr->size_message - 4] = 
	  p_ad->total_cost ;
	mpi_data_ptr->s_cost_message[main_mpi_data_ptr->size_message - 3] =
	  p_ad->nb_iter ;
      /* Update last info... nb swap, etc. */
	send_log_n(p_ad->main_mpi_data_ptr->size_message,
		   mpi_data_ptr->s_cost_message,
		   LS_CONFIG) ;
      }
    } /* take_sol */
#endif /* COMM_CONFIG */
  } /* if( (p_ad->nb_iter % count_to_communication)==0 ) { */
  return 0 ;
}

void
dead_end_final()
{
  int flag = 0 ;          /* if operation completed */

  /* free the_message */
  free( Gthe_message ) ;
  /* Cancel pending messages */
  while( list_recv_msgs.next != NULL ) {
    Gthe_message = get_tegami_from( &list_recv_msgs) ;
    /* Already recvd... and treated */
    MPI_Test( &(Gthe_message->handle), &flag, MPI_STATUS_IGNORE ) ;
    if( flag != 1 ) {
      TDPRINTF("%d launches MPI_Cancel()\n", my_num) ;
      MPI_Cancel( &(Gthe_message->handle) ) ;
      MPI_Wait( &(Gthe_message->handle), MPI_STATUS_IGNORE ) ;
      TDPRINTF("%d finished waiting of canceling\n", my_num) ;
    }
#if defined COMM_CONFIG
    free( Gthe_message->message ) ;
#endif
    free( Gthe_message ) ;
  }

  while( list_allocated_msgs.next != NULL ) {
    Gthe_message = get_tegami_from( &list_allocated_msgs) ;
#if defined COMM_CONFIG
    free( Gthe_message->message ) ;
#endif
    free( Gthe_message ) ;
  }
  while( list_sent_msgs.next != NULL ) {
    Gthe_message = get_tegami_from( &list_sent_msgs) ;
    /*    MPI_Cancel( &(the_message->handle) ) ;
	  MPI_Wait( &(the_message->handle), MPI_STATUS_IGNORE ) ;
    */
#if defined COMM_CONFIG
    free( Gthe_message->message ) ;
#endif
    free( Gthe_message ) ;
  }
  
  /* free all remainding structures */
  /* TODO: free s_cost_message */

  /* Arimasu ka? */
  
  TPRINTF("%d MPI_Finalize()...\n", my_num) ;
  MPI_Finalize() ;
  TPRINTF("%d MPI_Finalize() done\n", my_num) ;
}

/* msg must be same type than message in ad_solver_MPI.h */
void
send_log_n( unsigned int size_message,
	    unsigned int * msg,
	    protocol_msg tag_mpi )
{
  unsigned int i, range ;
  unsigned int nb_steps ;
  tegami * message ;
  unsigned int destination_node ;

#if defined DEBUG_MPI
  unsigned int sentNodes[NBMAXSTEPS] ; /* In fact, Log2(n)! */
#endif

  /* TODO: We should try to aggregate some sent_msg */

  /* Copy */
  range = msg[0] ;

#if defined STATS
  if( range == mpi_size )
    Gmpi_stats.nb_sent_mymessages++ ;
#endif

  /* Use Log(n) algo */
  nb_steps = ceil(log2(range)) ;

#if defined DEBUG_MPI
  switch( tag_mpi ) {
  case LS_KILLALL:
    TDPRINTF("Proc %d sending \"%s\" with "
	    "Range %d ; NB_steps %d ; vainqueur %d | (length  %d)\n",
	     my_num,
	     protocole_name[tag_mpi],
	     range,
	     nb_steps,
	     msg[1],
	     size_message) ;
    break ;
  case LS_COST:
  case LS_ITER:
    TDPRINTF("Proc %d sending \"%s\" with "
	     "Range %d ; NB_steps %d ; cost/iter %d | (length  %d)\n",
	     my_num,
	     protocole_name[tag_mpi],
	     range,
	     nb_steps,
	     msg[1],
	     size_message) ;
    break ;
  case LS_COST_ITER:
    TDPRINTF("Proc %d sending \"%s\" with "
	     "Range %d ; NB_steps %d ; cost %d ; iter %d | (length  %d)\n",
	     my_num,
	     protocole_name[tag_mpi],
	     range,
	     nb_steps,
	     msg[1],
	     msg[2],
	     size_message) ;
    break ;
  case LS_CONFIG:
    TDPRINTF("Proc %d sending \"%s\" with "
	     "Range %d ; NB_steps %d ; cost %d ; iter %d | (length  %d)\n",
	     my_num,
	     protocole_name[tag_mpi],
	     range,
	     nb_steps,
	     msg[size_message-4],
	     msg[size_message-3],
	     size_message) ;
    break ;
  default:
    TDPRINTF("Proc %d sending protocol %d... Undefined!\n"
	     " with Range %d ; NB_steps %d | (length  %d)\n\n",
	     my_num,
	     tag_mpi,
	     range,
	     nb_steps,
	     size_message) ;
    exit(0) ;
  }
#endif

  for( i=1 ; i<= nb_steps ; ++i ) {
    message = get_tegami_from( &list_allocated_msgs ) ;
    /* YC: FIXME. I don't get why we have to for() and memcpy doesnt work */
    /*    memcpy(message->message, msg, size_message) ; */
    for( int j=0 ; j<size_message ; ++j ) {
      message->message[j] = msg[j] ;
      /*  printf("->[%d]: %d | msg[%d]: %d\n",
      j,
      message->message[j],
      j,
      msg[j]) ; */
    }

    TDPRINTF("Fake: Proc %d sends msg (config;%d|%d;%d|%d) protocol %s range %d | l %d!\n",
	     my_num,
	     message->message[size_message-4], msg[size_message-4],
	     message->message[size_message-3], msg[size_message-3],
	     protocole_name[tag_mpi],
	     message->message[0],
	     size_message);
    /* Range that we'll send */
    message->message[0] = floor(range/2.0) ;
    /* Send to */
    destination_node = (my_num + (int)ceil(range/2.0)) % mpi_size ;
    /* For next iteration */
    range = range - message->message[0] ;

#if defined DEBUG_MPI
    /* Send to ... */
    sentNodes[i-1] = destination_node ;
#if defined COMM_COST
    TDPRINTF("Proc %d sends msg %d protocol %s range %d to %d!\n",
	     my_num,
	     message->message[1],
	     protocole_name[tag_mpi],
	     message->message[0],
	     sentNodes[i-1]) ;
#elif defined ITER_COST
    TDPRINTF("Proc %d sends msg (%d;%d) protocol %s range %d to %d!\n",
	     my_num,
	     message->message[1],
	     message->message[2],
	     protocole_name[tag_mpi],
	     message->message[0],
	     sentNodes[i-1]) ;
#elif defined COMM_CONFIG
    TDPRINTF("Proc %d sends msg (config;%d|%d;%d|%d) protocol %s!\n",
	     my_num,
	     message->message[size_message-4],	     msg[size_message-4],
	     message->message[size_message-3], 	     msg[size_message-3],
	     protocole_name[tag_mpi]) ;
#endif
#endif /* DEBUG_MPI */

#if defined STATS
    Gmpi_stats.nb_sent_messages++ ;
#endif
    /* Sends */
    MPI_Isend(message->message, size_message, MPI_INT,
	      destination_node,
	      tag_mpi, MPI_COMM_WORLD, &(message->handle)) ;
    push_tegami_on( message, &list_sent_msgs) ;
  }
#if defined DEBUG_MPI
  DPRINTF("-- %d sent to [", my_num) ;
  for( i=0 ; i< nb_steps ; ++i )
    DPRINTF(" %d ", sentNodes[i] ) ;
  DPRINTF("] \n" ) ;
#endif
}

