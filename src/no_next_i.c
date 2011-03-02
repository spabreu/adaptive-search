/*
 *  Adaptive search
 *
 *  Copyright (C) 2002-2011 Daniel Diaz, Philippe Codognet and Salvador Abreu
 *			MPI Yves Caniou and Florian Richoux
 *
 *  no_next_i.c: wrapper when user function Next_I is not defined
 */

int Next_I(int i)
{
  return i + 1;
}
