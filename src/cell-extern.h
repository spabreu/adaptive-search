// == AS SPU back end =========================================================
//
// -- Salvador Abreu, 2009 ----------------------------------------------------
//

#ifndef __CELL_EXTERN_H
#define __CELL_EXTERN_H 1


int as_mbx_avail ();
int as_mbx_read  ();

int  as_mbx_send_next (int v);
void as_mbx_send_all  (int v);

void as_mbx_copy_prev ();


#endif // __CELL_EXTERN_H
