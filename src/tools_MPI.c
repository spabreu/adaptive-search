/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  tools_MPI.h: MPI-specific utilities header file
 */

#include "tools_MPI.h"

#if defined PRINT_COSTS
/* Read n entries */
int
readn( int fd, char * buffer, int n )
{
  int nread;
  int nleft;
  char * ptr;

  ptr = buffer ;
  nleft = n;
  while( nleft != 0 ) {
    if( (nread = read(fd, ptr, nleft)) < 0 ) {
      if( nread < 0 )       // ERROR
	switch(errno) {
	case EBADF:
	  printf("File descriptor unvalid or not open for reading") ;
	  return 0 ;
	case EFAULT:
	  printf("Buffer pointe en dehors de l'espace d'adressage") ;
	  return 0 ;
	case EINVAL:
	  printf("EINVAL") ;
	  return 0 ;
	case EIO:
	  printf("EIO") ;
	  return 0 ;
	default:
	  printf("Undefined") ;
	  return 0 ;
	}
	return( 0 ) ;
    } else if( nread == 0 ) /* EOF */
      break ;
    nleft -= nread;
    ptr += nread;
  }
  return( n-nleft ) ;
}
/* Write n entries */
size_t
writen( int fd, const char * buffer, size_t n )
{
  size_t nleft;
  size_t nwritten;
  const char * ptr;
  
  ptr = buffer ;
  nleft = n ;
  while( nleft > 0 ) {
    if( (nwritten = write(fd, ptr, nleft)) <= 0 ) {
      if( errno == EINTR )
	nwritten = 0 ;	/* and call write() again */
      else
	return( 0 ) ;	/* error */
    }
    nleft -= nwritten ;
    ptr   += nwritten ;
  }
  return(n) ;
}
#endif /* PRINT_COSTS */

/* Insert the item at head of list */
void
push_tegami_on( tegami * msg, tegami * head )
{
  msg->next = head->next ;
  if( head->next != NULL ) {
    (head->next)->previous = msg ;
  } else head->previous = msg ;
  msg->previous = NULL ;
  head->next = msg ;
#ifdef YC_DEBUG_QUEUE
  head->size++ ;
  if( head->nb_max_msgs_used < head->size )
    head->nb_max_msgs_used = head->size ;
#endif
#ifdef YC_DEBUG_PRINT_QUEUE
  DPRINTF("Stack %s: (+1->) %d elts; max_msg=%d\n",
	  head->text, head->size, head->nb_max_msgs_used)
#endif
}
/* Drop and return the head of the list */
tegami *
get_tegami_from( tegami * head )
{
  tegami * item = head->next ;
#ifdef YC_DEBUG_QUEUE
  assert( head->next != NULL ) ;
#endif
  head->next = item->next ;
  head->previous = item->previous ;
#ifdef YC_DEBUG_QUEUE
  head->size-- ;
  assert( head->size >= 0 ) ;
#endif
#ifdef YC_DEBUG_PRINT_QUEUE
  DPRINTF("Stack %s: (-1->) %d elts\n", head->text, head->size)
#endif
  return item ;
}
tegami *
unqueue_tegami_from( tegami * head )
{
  tegami * item = head->previous ;
  if( item->previous != NULL ) {
    (item->previous)->next = NULL ;
  } else {
    head->next = NULL ;
  }
  head->previous = item->previous ;
#ifdef YC_DEBUG_QUEUE
  head->size-- ;
  assert( head->size >= 0 ) ;
#endif
#ifdef YC_DEBUG_PRINT_QUEUE
  DPRINTF("Stack %s: (-1->) %d elts\n", head->text, head->size)
#endif
  return item ;
}
