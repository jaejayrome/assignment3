/*--------------------------------------------------------------------*/
/* chunkbase.c                                                        */
/* Author: Donghwi Kim, KyoungSoo Park                                */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "chunk.h"

// get footer operation
static Chunk_T get_chunk_footer(Chunk_T c) {
   Chunk_T footer = c + c-> units + 1;
   return footer;
}

/*--------------------------------------------------------------------*/
int chunk_get_status(Chunk_T c)
{
   return c->status;
}
/*--------------------------------------------------------------------*/
void chunk_set_status(Chunk_T c, int status)
{
   c->status = status;
}
/*--------------------------------------------------------------------*/
int chunk_get_units(Chunk_T c)
{
   return c->units;
}
/*--------------------------------------------------------------------*/
void chunk_set_units(Chunk_T c, int units)
{
   c->units = units;
}
/*--------------------------------------------------------------------*/
Chunk_T
chunk_get_next_free_chunk(Chunk_T c)
{
   return c->next;
}
/*--------------------------------------------------------------------*/
void chunk_set_next_free_chunk(Chunk_T c, Chunk_T next)
{
   c->next = next;
}
/*--------------------------------------------------------------------*/
Chunk_T
chunk_get_prev_free_chunk(Chunk_T c)
{
   assert(c != NULL);

   Chunk_T footer = get_chunk_footer(c);

   /* Use the current footer's pointer to point to the previous free chunk's footer */
   Chunk_T prev_free_footer = footer->next;

   return prev_free_footer ? prev_free_footer : NULL;
}

/*--------------------------------------------------------------------*/
void chunk_set_prev_free_chunk(Chunk_T c, Chunk_T prev)
{
   /* To set the previous free chunk, modify the footer of the previous chunk */
   assert(c != NULL);
   assert(prev != NULL);

   Chunk_T footer = get_chunk_footer(c);
   footer->next = c;
}
/*--------------------------------------------------------------------*/
// returns next header 
Chunk_T
chunk_get_next_adjacent(Chunk_T c, void* start, void* end)
{
   Chunk_T n;

   assert((void *)c >= start);

   /* Note that a chunk consists of one chunk unit for a header, and
    * many chunk units for data and one chunk unit for a footer */
   n = c + c->units + 2;

   /* If 'c' is the last chunk in memory space, then return NULL. */
   if ((void *)n >= end)
      return NULL;
   
   return n;
}
/*--------------------------------------------------------------------*/
// returns prev header
Chunk_T
chunk_get_prev_adjacent(Chunk_T c, void *start, void *end)
{
   assert(c != NULL);
   assert(start != NULL);
   assert(end != NULL);

   // we move back by 1 since footer would also be 1 chunk unit
   Chunk_T prev_footer = c - 1;

   /* Ensure we are within bounds */
   if ((void *)prev_footer < start) {
       return NULL; /* The previous block is outside the memory range */
   }

   /* Now, go from the footer to the header of the previous chunk.
    * The header is located by moving back from the footer based on the chunk's size (units).
    */
   Chunk_T prev_header = prev_footer - prev_footer->units - 1;

   /* Ensure the header is within bounds */
   if ((void *)prev_header < start || (void *)prev_header >= end) {
       return NULL; /* The previous header is outside the memory range */
   }

   return prev_header;
}


#ifndef NDEBUG
/*--------------------------------------------------------------------*/
int chunk_is_valid(Chunk_T c, void *start, void *end)
/* Return 1 (TRUE) iff c is valid */
{
   assert(c != NULL);
   assert(start != NULL);
   assert(end != NULL);

   if (c < (Chunk_T)start)
   {
      fprintf(stderr, "Bad heap start\n");
      return 0;
   }
   if (c >= (Chunk_T)end)
   {
      fprintf(stderr, "Bad heap end\n");
      return 0;
   }
   if (c->units == 0)
   {
      fprintf(stderr, "Zero units\n");
      return 0;
   }
   return 1;
}
#endif
