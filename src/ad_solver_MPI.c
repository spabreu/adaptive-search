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
    "Sending cost"
  } ;

/*------------------*
 * Global variables *
 *------------------*/

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
Ad_Solve_init_MPI_data( Ad_Solve_MPIData * mpi_data )
{
  mpi_data->nb_block = 0 ;
#if (defined COMM_COST) || (defined ITER_COST)
  mpi_data->best_cost_sent = INT_MAX ;
  mpi_data->best_cost_received = INT_MAX ;
#endif /* COMM_COST || ITER_COST */
#if defined ITER_COST
  mpi_data->iter_of_best_cost_sent = INT_MAX ;
  mpi_data->mixed_received_msg_iter = -1 ;
#endif /* ITER_COST */
#if defined MIN_ITER_BEFORE_RESTART
  mpi_data->nbiter_since_restart = 0 ;
#endif
}

int
Ad_Solve_manage_MPI_communications( Ad_Solve_MPIData * mpi_data,
				    AdData * p_ad )
{
  unsigned int i ;
  /* .. if operation completed */
  int flag = 0 ;
  char results[RESULTS_CHAR_MSG_SIZE] ;
  /* # of received msgs in a block of iteration */
  unsigned int number_received_msgs ;
  tegami * tmp_tegami ;
#if (defined COMM_COST) || (defined ITER_COST)
  unsigned int ran_tmp ;
#endif /* COMM_COST || ITER_COST */
#if (defined(YC_DEBUG))||(defined(YC_DEBUG_MPI))||(defined(YC_DEBUG_RESTART))
  struct timeval tv ;
#endif

  if( (p_ad->nb_iter % count_to_communication)==0 ) {
    mpi_data->nb_block++ ;
    /* Try to free older messages in their sending order */
#if defined YC_DEBUG
    gettimeofday(&tv, NULL);
    DPRINTF("%ld:%ld --------------- %d,%d: Free older messages\n", 
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    mpi_data->nb_block) ;
#endif /* YC_DEBUG */
    flag = 1 ;
    while( (list_sent_msgs.next != NULL) && (flag!=0) ) {
#if defined YC_DEBUG_MPI
      gettimeofday(&tv, NULL);
      DPRINTF("%ld:%ld: Proc %d launches MPI_TEST()\n",
	      tv.tv_sec, tv.tv_usec,
	      my_num) ;
#endif /* YC_DEBUG_MPI */
      MPI_Test( &((list_sent_msgs.previous)->handle), &flag,
		MPI_STATUS_IGNORE ) ;
      if( flag != 0 ) {
	push_tegami_on( unqueue_tegami_from(&list_sent_msgs),
			&list_allocated_msgs) ;
      } /* if( flag != 0 ) { */
    } /* while( (list_sent_msgs.next != NULL) && (flag!=0) ) { */
#if defined YC_DEBUG
    DPRINTF("-------------- %d: Reception\n", my_num) ;
#endif	
    /*-------------------------------------------------------------*/
    /*************************** Reception *************************/
    /*-------------------------------------------------------------*/
    /************** Check how many msg were received ***************/
    number_received_msgs = 0 ;
#if (defined COMM_COST) || (defined ITER_COST)
    mixed_received_msg_cost = INT_MAX ;
#endif
    do {
#if defined YC_DEBUG_MPI
      gettimeofday(&tv, NULL);
      DPRINTF("%ld:%ld: Proc %d launches MPI_TEST()\n",
	      tv.tv_sec, tv.tv_usec,
	      my_num) ;
#endif
      MPI_Test(&(the_message->handle), &flag, &(the_message->status)) ;
      if( flag > 0 ) {                     /* We received at least one! */
#if defined YC_DEBUG_MPI
	gettimeofday(&tv, NULL);
	DPRINTF("%ld:%ld: %d received protocol %s from %d\n",
		tv.tv_sec, tv.tv_usec,
		my_num,
		protocole_name[the_message->status.MPI_TAG],
		the_message->status.MPI_SOURCE) ;
#endif
	/* Initiate treatment of all msg */
	number_received_msgs++ ;
	switch( the_message->status.MPI_TAG ) {
	  /******************************* LS_KILLALL *********************/
	case LS_KILLALL:
	  /* Then quits! */
#if defined YC_DEBUG
	  DPRINTF("LS_KILLALL from %d\n",
		  the_message->status.MPI_SOURCE) ;
#endif /* YC_DEBUG */
	  if( my_num == 0 ) {
	    /* Reuse msg and kill everyone */
	    the_message->message[0] = mpi_size ;
	    the_message->message[1] = the_message->status.MPI_SOURCE ;
	    send_log_n( the_message->message, LS_KILLALL ) ;
	    /* Recv results from source */
#if defined YC_DEBUG_MPI
	    gettimeofday(&tv, NULL);
	    DPRINTF("%ld.%ld: %d launches MPI_Irecv() of results for"
		    " source %d\n",
		    tv.tv_sec, tv.tv_usec, my_num,
		    the_message->status.MPI_SOURCE) ;
#endif
	    MPI_Recv( results, RESULTS_CHAR_MSG_SIZE, MPI_CHAR,
		      the_message->status.MPI_SOURCE, SENDING_RESULTS,
		      MPI_COMM_WORLD,
		      MPI_STATUS_IGNORE) ;
	    printf("%s\n", results) ;
#if defined PRINT_COSTS
	    print_costs() ;
#endif
	    MPI_Finalize() ;
	    dead_end_final() ;
	    exit(0) ;
	  } else {                                  /* Proc N */
	    /* S.o finished before me. I killall the ones I'm responsible */
	    send_log_n( the_message->message, LS_KILLALL ) ;
#if defined PRINT_COSTS
	    print_costs() ;
#endif
	    MPI_Finalize() ;
	    dead_end_final() ;
	    exit(0) ;
	  }
	  break ;
#if (defined COMM_COST) || (defined ITER_COST)
	  /******************************* LS_COST *********************/
	case LS_COST:     /* take the min! */
#  if defined YC_DEBUG
	  DPRINTF("%d takes the min between min'_recv(%d) and recvd(%d)\n",
		  my_num,
		  mixed_received_msg_cost, the_message->message[1]) ;
#  endif
	  if( mixed_received_msg_cost > the_message->message[1] ) {
	    mixed_received_msg_cost = the_message->message[1] ;
#  if defined ITER_COST
	    /* Save according iter */
	    mixed_received_msg_iter = the_message->message[2] ;
#  endif
	  }
	  /* Store recvd msg to treat it later */
	  push_tegami_on( the_message, &list_recv_msgs) ;
	  /* Launch new async recv */
	  the_message = get_tegami_from( &list_allocated_msgs) ;
#  if defined YC_DEBUG_MPI
	  gettimeofday(&tv, NULL);
	  DPRINTF("%ld.%ld: %d launches MPI_Irecv(), any source\n",
		  tv.tv_sec, tv.tv_usec, my_num) ;
#  endif
	      MPI_Irecv(&(the_message->message), SIZE_MESSAGE, MPI_INT,
			MPI_ANY_SOURCE, 
			MPI_ANY_TAG,
			MPI_COMM_WORLD, &(the_message->handle)) ;
	      break ;
#endif /* COMM_COST || ITER_COST */
	case SENDING_RESULTS:
	default:
	  printf("Should never happen! Exiting.\n") ;
	  exit(-1) ;
	} /*  switch( the_message->status.MPI_TAG ) { */
      } /* if flag */
    } while( flag > 0 ) ;
#if defined YC_DEBUG
    DPRINTF("%d received %d messages in total this time\n",
	    my_num,
	    number_received_msgs) ;
    DPRINTF("-------------- %d: Treatment\n", my_num) ;
#endif	
    /****************** Treat all received msgs (except LS_KILLALL) **/
    for( i=0 ; i<number_received_msgs ; i++ ) {
      tmp_tegami = get_tegami_from( &list_recv_msgs) ;
      switch( tmp_tegami->status.MPI_TAG ) {
#if (defined COMM_COST) || (defined ITER_COST)
	/******************************* LS_COST *********************/
      case LS_COST:
#  if defined YC_DEBUG_MPI
	gettimeofday(&tv, NULL);
	DPRINTF("%ld:%ld: %d treats LS_COST of %d from %d\n",
		tv.tv_sec, tv.tv_usec,
		my_num,
		tmp_tegami->message[1],
		tmp_tegami->status.MPI_SOURCE) ;
#  endif /* YC_DEBUG_MPI */
	/* Crash cost with our value and avoids a test */
	tmp_tegami->message[1] = mixed_received_msg_cost ;
#  if defined ITER_COST
	tmp_tegami->message[2] = mixed_received_msg_iter ;
#  endif
	/* [cont. to] Distribute the information */
	send_log_n( tmp_tegami->message, LS_COST ) ;
	/* Msg received and treated */
	push_tegami_on( tmp_tegami, &list_allocated_msgs) ;
	break;
#endif /* COMM_COST || ITER_COST */
      default:
	printf("This should never happen! Exiting...\n") ;
	exit(-1) ;
      } /* switch( the_message->status.MPI_TAG ) { */
    } /* for( i=0 ; i<number_received_msgs ; i++ ) { */
#if defined YC_DEBUG
    DPRINTF("-------------- %d: Impact\n", my_num) ;
#endif	
    /****************** Repercusion of messages on me *************/
#if (defined COMM_COST) || (defined ITER_COST)
    if( number_received_msgs > 0 ) {
      /** Do we take into account the received cost? **/
      if( mixed_received_msg_cost < best_cost_received ) {
	best_cost_received = mixed_received_msg_cost ;
#  if defined ITER_COST
	iter_for_best_cost_received = mixed_received_msg_iter ;
#  endif
      }
#  if defined YC_DEBUG
      DPRINTF("%d: Best recv cost is now %d\n",
	      my_num, best_cost_received) ;
#    if defined ITER_COST
      DPRINTF("... with #iter %d\n", iter_for_best_cost_received ) ;
#    endif
#  endif
      /******************************* COMM_COST ****************/
#  if defined COMM_COST
      if( (unsigned)p_ad->total_cost > mixed_received_msg_cost ) {
	ran_tmp=(((float)random())/RAND_MAX)*100 ;
#    if defined YC_DEBUG	  
	DPRINTF("Proc %d (cost %d > %d): ran=%d >?< %d\n",
		my_num,
		p_ad->total_cost,
		mixed_received_msg_cost,
		ran_tmp,
		proba_communication) ;
#    endif
	
	if( ran_tmp < proba_communication ) {
#    if defined YC_DEBUG_RESTART
	  gettimeofday(&tv, NULL);
	  DPRINTF("%ld:%ld: Proc %d restarts!\n",
		  tv.tv_sec, tv.tv_usec,
		  my_num) ;
#    endif
	  return 10 ;
	} /* if( tan_tmp */  
      } /* if( p_ad->total.cost */
#  endif /* COMM_COST */
      /******************************* ITER_COST ****************/
#  if defined ITER_COST
      /* Best cost AND smaller #iter */
      if( (unsigned)p_ad->total_cost > mixed_received_msg_cost ) {
	if( mixed_received_msg_iter < p_ad->nb_iter ) {
	  
	  ran_tmp=(((float)random())/RAND_MAX)*100 ;
#    if defined YC_DEBUG	  
	  DPRINTF("Proc %d (cost %d > %d): ran=%d >?< %d\n",
		  my_num,
		  p_ad->total_cost,
		  mixed_received_msg_cost,
		  ran_tmp,
		  proba_communication) ;
#    endif
	  if( ran_tmp < proba_communication ) {
#    if defined YC_DEBUG_RESTART
	    gettimeofday(&tv, NULL);
	    DPRINTF("%ld:%ld: Proc %d restarts!\n",
		    tv.tv_sec, tv.tv_usec,
		    my_num) ;
#    endif
	    return 10 ;
	  } /* if( tan_tmp */  
	}/* if( mixed_recv */
      } /* if( p_ad->total.cost */
#  endif /* ITER_COST */
    } /* if( number_received_msgs > 0 ) { */
#endif /* COMM_COST || ITER_COST */
#if defined YC_DEBUG
    DPRINTF("-------------- %d: Sending\n", my_num) ;
#endif	
    /*-------------------------------------------------------------*/
    /**************************** Sending **************************/
    /*-------------------------------------------------------------*/
#if (defined COMM_COST) || (defined ITER_COST)
    /************** Sends best cost? ************/
    if( (unsigned)best_cost < best_cost_sent ) {
#  if defined YC_DEBUG
      DPRINTF("Proc %d sends cost %d\n", my_num, best_cost) ;
#  endif /* YC_DEBUG */
      s_cost_message[0] = mpi_size ;
      s_cost_message[1] = best_cost ;
#  if defined ITER_COST
      s_cost_message[1] = p_ad->nb_iter ;
#  endif
      send_log_n(s_cost_message, LS_COST) ;	      
      best_cost_sent = best_cost ;
#  if defined ITER_COST
      iter_of_best_cost_sent = p_ad->nb_iter ;
#  endif
    } /* if( best_cost < best_cost_sent ) { */
#endif /* COMM_COST || ITER_COST */
  } /* if( (p_ad->nb_iter % count_to_communication)==0 ) { */
  return 0 ;
}



void
dead_end_final()
{
  int flag = 0 ;          /* .. if operation completed */
  struct timeval tv ;

  return ;

  /*  printf("MPI_Finalize() sans free\n") ;
  MPI_Finalize() ;
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize()\n",
           tv.tv_sec, tv.tv_usec, my_num) ;
  free_data_of_ad_solver() ;
  return ;
  */

  /* free the_message */
  free( the_message ) ;
  /* Cancel pending messages */
  while( list_recv_msgs.next != NULL ) {
    the_message = get_tegami_from( &list_recv_msgs) ;
    /* Already recvd... and treated */
    MPI_Test( &(the_message->handle), &flag, MPI_STATUS_IGNORE ) ;
    if( flag != 1 ) {
#if defined DEBUG_MPI_ENDING
    gettimeofday(&tv, NULL);
    DPRINTF("%ld.%ld: %d launches MPI_Cancel()\n",
	    tv.tv_sec, tv.tv_usec, my_num) ;
#endif
      MPI_Cancel( &(the_message->handle) ) ;
      MPI_Wait( &(the_message->handle), MPI_STATUS_IGNORE ) ;
#if defined DEBUG_MPI_ENDING
      gettimeofday(&tv, NULL);
      DPRINTF("%ld.%ld: %d finished waiting of canceling\n",
	      tv.tv_sec, tv.tv_usec, my_num) ;
#endif

    }
    /*    free( the_message ) ; */
  }

  while( list_allocated_msgs.next != NULL ) {
    the_message = get_tegami_from( &list_allocated_msgs) ;
    free( the_message ) ;
  }
  while( list_sent_msgs.next != NULL ) {
    the_message = get_tegami_from( &list_sent_msgs) ;
    /*    MPI_Cancel( &(the_message->handle) ) ;
	  MPI_Wait( &(the_message->handle), MPI_STATUS_IGNORE ) ;
    */
    free( the_message ) ;
  }
  /* free all remainding structures */
  /* Arimasu ka? */
  
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize()...\n",
	 tv.tv_sec, tv.tv_usec, my_num) ;
  MPI_Finalize() ;
  gettimeofday(&tv, NULL);
  printf("%ld.%ld: %d MPI_Finalize() done\n",
	 tv.tv_sec, tv.tv_usec, my_num) ;
}

/* msg must be same type than message in ad_solver.h */
/* msg of size SIZE_MESSAGE */
void
send_log_n( unsigned int * msg, protocol_msg tag_mpi )
{
  unsigned int i, range ;
  unsigned int nb_steps ;
  tegami * message ;
  unsigned int destination_node ;

#if defined YC_DEBUG_MPI
  struct timeval tv ;
  unsigned int sentNodes[NBMAXSTEPS] ; /* In fact, Log2(n)! */
#endif

  /* TODO: We should try to aggregate some sent_msg */

  /* Copy */
  range = msg[0] ;

  /* Use Log(n) algo */
  nb_steps = ceil(log2(range)) ;

#if defined YC_DEBUG_MPI
  gettimeofday(&tv, NULL);
  switch( tag_mpi ) {
  case LS_KILLALL:
    DPRINTF("%ld:%ld: Proc %d sending \"%s\" with "
	    "Range %d ; NB_steps %d ; vainqueur %d\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    protocole_name[tag_mpi],
	    range,
	    nb_steps,
	    msg[1]) ;
    break ;
  case LS_COST:
    DPRINTF("%ld:%ld: Proc %d sending \"%s\" with "
	    "Range %d ; NB_steps %d ; cost %d\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    protocole_name[tag_mpi],
	    range,
	    nb_steps,
	    msg[1]) ;
    break ;
  default:
    printf("Undefined !") ;
  }
#endif

  for( i=1 ; i<= nb_steps ; ++i ) {
    message = get_tegami_from( &list_allocated_msgs) ;

#if defined ITER_COST
    /* #iter */
    message->message[2] = msg[2] ;
#endif
    /* Winner */    
    message->message[1] = msg[1] ;
    /* Range that we'll send */
    message->message[0] = floor(range/2.0) ;
    /* Send to */
    destination_node = (my_num + (int)ceil(range/2.0)) % mpi_size ;
    /* For next iteration */
    range = range - message->message[0] ;

#if defined YC_DEBUG_MPI
    /* Send to ... */
    gettimeofday(&tv, NULL);
    sentNodes[i-1] = destination_node ;
#if defined COMM_COST
    DPRINTF("%ld:%ld: Proc %d sends msg %d protocol %s range %d to %d !\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    message->message[1],
	    protocole_name[tag_mpi],
	    message->message[0],
	    sentNodes[i-1]) ;
#elif defined ITER_COST
    /* Send to ... */
    gettimeofday(&tv, NULL);
    sentNodes[i-1] = destination_node ;
    DPRINTF("%ld:%ld: Proc %d sends msg (%d;%d) protocol %s range %d to %d !\n",
	    tv.tv_sec, tv.tv_usec,
	    my_num,
	    message->message[1],
	    message->message[2],
	    protocole_name[tag_mpi],
	    message->message[0],
	    sentNodes[i-1]) ;
#endif
#endif /* YC_DEBUG_MPI */
    /* Sends */
    MPI_Isend(message->message, SIZE_MESSAGE, MPI_INT,
	      destination_node,
	      tag_mpi, MPI_COMM_WORLD, &(message->handle)) ;
    push_tegami_on( message, &list_sent_msgs) ;
  }
#if defined YC_DEBUG_MPI
  DPRINTF("-- %d sent to [", my_num) ;
  for( i=0 ; i< nb_steps ; ++i )
    DPRINTF(" %d ", sentNodes[i] ) ;
  DPRINTF("] \n" ) ;
#endif
}

