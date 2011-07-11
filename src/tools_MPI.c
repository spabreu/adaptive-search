/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2010 Daniel Diaz, Philippe Codognet and Salvador Abreu 
 *  Author:                                                               
 *    - Yves Caniou (yves.caniou@ens-lyon.fr)                               
 *  tools_MPI.h: MPI-specific utilities header file
 */

#include "tools_MPI.h"

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
#ifdef DEBUG_QUEUE
  head->size++ ;
  if( head->nb_max_msgs_used < head->size )
    head->nb_max_msgs_used = head->size ;
#endif
#ifdef DEBUG_PRINT_QUEUE
  DPRINTF("Push on %s: (+1->) %d elts; max_msg=%d\n",
	  head->text, head->size, head->nb_max_msgs_used) ;
#endif
}
/* Drop and return the head of the list */
tegami *
get_tegami_from( tegami * head )
{
  tegami * item = head->next ;
#ifdef DEBUG_QUEUE
  assert( head->next != NULL ) ;
#endif
  head->next = item->next ;
  if( head->next == NULL )
    head->previous = NULL ;
#ifdef DEBUG_QUEUE
  head->size-- ;
  assert( head->size >= 0 ) ;
#endif
#ifdef DEBUG_PRINT_QUEUE
  DPRINTF("Get from %s: (-1->) %d elts\n", head->text, head->size) ;
#endif
  return item ;
}
/* Drop and return the last elt of the list */
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
#ifdef DEBUG_QUEUE
  head->size-- ;
  assert( head->size >= 0 ) ;
#endif
#ifdef DEBUG_PRINT_QUEUE
  DPRINTF("Unqueue from %s: (-1->) %d elts\n", head->text, head->size) ;
#endif
  return item ;
}
