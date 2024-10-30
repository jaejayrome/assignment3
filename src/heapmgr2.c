#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "chunk.h"
#include "heapmgr.h"

#define FALSE 0
#define TRUE 1

/* Size calculation macros */
#define FOOTER_SIZE (sizeof(struct ChunkFooter))
#define FOOTER_UNITS ((FOOTER_SIZE + CHUNK_UNIT - 1) / CHUNK_UNIT)
#define MIN_SPLIT_UNITS (2 + FOOTER_UNITS)

/* Modified bin configuration for exponential growth */
#define NUM_SMALL_BINS 32 /* For sizes up to 512 bytes */
#define NUM_LARGE_BINS 32 /* For larger sizes */
#define TOTAL_BINS (NUM_SMALL_BINS + NUM_LARGE_BINS)

/* Forward declarations of static functions */
static int get_bin_index(size_t size);
static void init_bins(void);
static void insert_into_bin(Chunk_T chunk);
static void remove_from_bin(Chunk_T chunk);
static Chunk_T find_chunk(size_t size);
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2);

static Chunk_T g_bins[TOTAL_BINS];
static void *g_heap_start = NULL, *g_heap_end = NULL;

/* Get appropriate bin for a given size */
static int get_bin_index(size_t size)
{
   if (size <= CHUNK_UNIT * 2)
   { /* 1-32 bytes */
      return 0;
   }

   /* Use exponential ranges: 1-2, 3-4, 5-8, 9-16, 17-32, ... */
   size_t bin_size = 2;
   int bin = 0;

   while (bin < TOTAL_BINS - 1)
   {
      if (size <= bin_size * CHUNK_UNIT)
      {
         return bin;
      }
      bin_size *= 2;
      bin++;
   }

   return TOTAL_BINS - 1; /* Largest bin */
}

static int is_valid_chunk_addr(void *addr)
{
   return (addr >= g_heap_start && addr < g_heap_end);
}

static int is_valid_footer_addr(void *footer_addr, Chunk_T c)
{
   return (footer_addr > (void *)c &&
           footer_addr >= g_heap_start &&
           footer_addr < g_heap_end &&
           ((size_t)footer_addr - (size_t)c) >= (CHUNK_UNIT + sizeof(struct ChunkFooter)));
}

/* Initialize all bins */
static void init_bins()
{
   for (int i = 0; i < TOTAL_BINS; i++)
   {
      g_bins[i] = NULL;
   }
}

/* Insert chunk into appropriate bin */
static void insert_into_bin(Chunk_T chunk)
{
   if (!chunk || !is_valid_chunk_addr(chunk))
   {
      return;
   }

   /* Validate chunk size */
   size_t units = chunk_get_units(chunk);
   if (units <= 0 || units > ((size_t)(g_heap_end - g_heap_start) / CHUNK_UNIT))
   {
      return;
   }

   /* Calculate and validate footer location */
   Footer_T footer = (Footer_T)((char *)chunk + (units * CHUNK_UNIT) + CHUNK_UNIT);
   if (!is_valid_footer_addr(footer, chunk))
   {
      return;
   }

   int bin_index = get_bin_index(units * CHUNK_UNIT);
   if (bin_index < 0 || bin_index >= TOTAL_BINS)
   {
      return;
   }

   /* Insert at head of appropriate bin */
   chunk->prev = NULL;
   chunk->next = g_bins[bin_index];
   if (g_bins[bin_index])
   {
      g_bins[bin_index]->prev = chunk;
   }
   g_bins[bin_index] = chunk;

   chunk_set_status(chunk, CHUNK_FREE);

   /* Set footer carefully */
   if (is_valid_footer_addr(footer, chunk))
   {
      footer->header = chunk;
   }
}

/* Remove chunk from its bin */
static void remove_from_bin(Chunk_T chunk)
{
   if (chunk->prev)
   {
      chunk->prev->next = chunk->next;
   }
   else
   {
      /* This was the head of its bin */
      int bin_index = get_bin_index(chunk_get_units(chunk) * CHUNK_UNIT);
      g_bins[bin_index] = chunk->next;
   }

   if (chunk->next)
   {
      chunk->next->prev = chunk->prev;
   }

   chunk->prev = chunk->next = NULL;
   chunk_set_status(chunk, CHUNK_IN_USE);
   chunk_set_footer(chunk);
}

/* Find best fit chunk in bins */
static Chunk_T find_chunk(size_t size)
{
   int bin_index = get_bin_index(size);
   size_t required_units = (size + CHUNK_UNIT - 1) / CHUNK_UNIT;

   /* First try exact bin */
   Chunk_T chunk = g_bins[bin_index];
   while (chunk)
   {
      if (chunk_get_units(chunk) >= required_units)
      {
         remove_from_bin(chunk);
         return chunk;
      }
      chunk = chunk->next;
   }

   /* Then try larger bins */
   for (int i = bin_index + 1; i < TOTAL_BINS; i++)
   {
      chunk = g_bins[i];
      if (chunk)
      {
         remove_from_bin(chunk);
         return chunk;
      }
   }

   return NULL;
}

/* Malloc implementation */
/* Modified malloc implementation with better large memory handling */
void *heapmgr_malloc(size_t size)
{
   static int is_init = FALSE;
   Chunk_T chunk;
   size_t units;

   if (size <= 0)
      return NULL;

   if (!is_init)
   {
      init_bins();
      g_heap_start = g_heap_end = sbrk(0);
      is_init = TRUE;
   }

   /* Calculate required units */
   units = (size + CHUNK_UNIT - 1) / CHUNK_UNIT;

   /* Try to find suitable chunk in bins */
   chunk = find_chunk(size);
   if (chunk)
   {
      /* Split if possible */
      if (chunk_get_units(chunk) > units + MIN_SPLIT_UNITS)
      {
         Chunk_T new_chunk = (Chunk_T)((char *)chunk +
                                       (units + 1) * CHUNK_UNIT + FOOTER_SIZE);

         chunk_set_units(new_chunk, chunk_get_units(chunk) - units - 1 - FOOTER_UNITS);
         chunk_set_units(chunk, units);

         /* Split into smaller chunks if the remaining size is large */
         size_t remaining_size = chunk_get_units(new_chunk) * CHUNK_UNIT;
         if (remaining_size > 4096)
         {                                           /* 4KB threshold */
            size_t optimal_size = 2048 / CHUNK_UNIT; /* 2KB chunks */
            Chunk_T current = new_chunk;

            while (chunk_get_units(current) > optimal_size + MIN_SPLIT_UNITS)
            {
               size_t split_units = optimal_size;
               Chunk_T split_chunk = (Chunk_T)((char *)current +
                                               (split_units + 1) * CHUNK_UNIT + FOOTER_SIZE);

               chunk_set_units(split_chunk, chunk_get_units(current) - split_units - 1 - FOOTER_UNITS);
               chunk_set_units(current, split_units);

               insert_into_bin(current);
               current = split_chunk;
            }
            insert_into_bin(current);
         }
         else
         {
            insert_into_bin(new_chunk);
         }
      }
      return (void *)((char *)chunk + CHUNK_UNIT);
   }

   /* Allocate new memory with pre-splitting strategy */
   size_t min_size = 4096 / CHUNK_UNIT; /* 4KB minimum allocation */
   size_t alloc_units = units > min_size ? units : min_size;

   chunk = sbrk((alloc_units + 1) * CHUNK_UNIT + FOOTER_SIZE);
   if (chunk == (void *)-1)
      return NULL;

   g_heap_end = sbrk(0);

   /* Initialize new chunk */
   chunk_set_units(chunk, alloc_units);
   chunk_set_status(chunk, CHUNK_IN_USE);
   chunk_set_footer(chunk);

   /* Pre-split large allocations into smaller chunks */
   if (alloc_units > units + MIN_SPLIT_UNITS)
   {
      size_t remaining = alloc_units - units - 1 - FOOTER_UNITS;
      size_t optimal_size = 2048 / CHUNK_UNIT; /* 2KB chunks */

      Chunk_T current = (Chunk_T)((char *)chunk + (units + 1) * CHUNK_UNIT + FOOTER_SIZE);
      chunk_set_units(chunk, units);
      chunk_set_footer(chunk);

      while (remaining > optimal_size + MIN_SPLIT_UNITS)
      {
         Chunk_T next = (Chunk_T)((char *)current + (optimal_size + 1) * CHUNK_UNIT + FOOTER_SIZE);

         chunk_set_units(current, optimal_size);
         insert_into_bin(current);

         remaining -= optimal_size + 1 + FOOTER_UNITS;
         current = next;
      }

      if (remaining > 0)
      {
         chunk_set_units(current, remaining);
         insert_into_bin(current);
      }
   }

   return (void *)((char *)chunk + CHUNK_UNIT);
}

static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2)
{
   if (!c1 || !c2 || !is_valid_chunk_addr(c1) || !is_valid_chunk_addr(c2) || c1 >= c2)
   {
      return c1;
   }

   size_t units1 = chunk_get_units(c1);
   size_t units2 = chunk_get_units(c2);

   /* Validate chunk sizes */
   if (units1 <= 0 || units2 <= 0 ||
       units1 > ((size_t)(g_heap_end - (void *)c1) / CHUNK_UNIT) ||
       units2 > ((size_t)(g_heap_end - (void *)c2) / CHUNK_UNIT))
   {
      return c1;
   }

   /* Verify chunks are adjacent */
   if ((char *)c1 + (units1 + 1) * CHUNK_UNIT + FOOTER_SIZE != (char *)c2)
   {
      return c1;
   }

   /* Verify both chunks are free */
   if (chunk_get_status(c1) != CHUNK_FREE || chunk_get_status(c2) != CHUNK_FREE)
   {
      return c1;
   }

   /* Calculate new size */
   size_t total_units = units1 + units2 + 1 + FOOTER_UNITS;

   /* Validate final size */
   if (total_units > ((size_t)(g_heap_end - (void *)c1) / CHUNK_UNIT))
   {
      return c1;
   }

   /* Update chunk */
   chunk_set_units(c1, total_units - 1 - FOOTER_UNITS);

   /* Validate and set footer */
   Footer_T footer = (Footer_T)((char *)c1 + ((total_units - 1 - FOOTER_UNITS) * CHUNK_UNIT) + CHUNK_UNIT);
   if (is_valid_footer_addr(footer, c1))
   {
      footer->header = c1;
   }

   return c1;
}

/* Free implementation reuses optimized version from heapmgr1 with bin modifications */
void heapmgr_free(void *ptr)
{
   if (!ptr || !is_valid_chunk_addr(ptr))
   {
      return;
   }

   Chunk_T chunk = (Chunk_T)((char *)ptr - CHUNK_UNIT);
   if (!is_valid_chunk_addr(chunk))
   {
      return;
   }

   /* Validate chunk */
   size_t chunk_units = chunk_get_units(chunk);
   if (chunk_units <= 0 ||
       chunk_units > ((size_t)(g_heap_end - g_heap_start) / CHUNK_UNIT) ||
       chunk_get_status(chunk) != CHUNK_IN_USE)
   {
      return;
   }

   /* Calculate footer location */
   Footer_T footer = (Footer_T)((char *)chunk + (chunk_units * CHUNK_UNIT) + CHUNK_UNIT);
   if (!is_valid_footer_addr(footer, chunk))
   {
      return;
   }

   /* Try to merge with neighbors */
   Chunk_T prev = NULL;
   Chunk_T next = NULL;

   /* Check for previous chunk */
   if ((void *)chunk > g_heap_start)
   {
      void *prev_footer_addr = (void *)((char *)chunk - sizeof(struct ChunkFooter));
      if (is_valid_chunk_addr(prev_footer_addr))
      {
         Footer_T prev_footer = (Footer_T)prev_footer_addr;
         prev = prev_footer->header;
         if (!is_valid_chunk_addr(prev) || prev >= chunk)
         {
            prev = NULL;
         }
      }
   }

   /* Check for next chunk */
   void *next_addr = (void *)((char *)footer + sizeof(struct ChunkFooter));
   if (is_valid_chunk_addr(next_addr))
   {
      next = (Chunk_T)next_addr;
      size_t next_units = chunk_get_units(next);
      if (next_units <= 0 ||
          next_units > ((size_t)((char *)g_heap_end - (char *)next) / CHUNK_UNIT))
      {
         next = NULL;
      }
   }

   /* Perform merges if possible */
   if (prev && chunk_get_status(prev) == CHUNK_FREE)
   {
      remove_from_bin(prev);
      if (is_valid_chunk_addr(prev) && is_valid_chunk_addr(chunk))
      {
         chunk = merge_chunk(prev, chunk);
      }
   }

   if (next && chunk_get_status(next) == CHUNK_FREE)
   {
      remove_from_bin(next);
      if (is_valid_chunk_addr(chunk) && is_valid_chunk_addr(next))
      {
         chunk = merge_chunk(chunk, next);
      }
   }

   /* Finally insert the chunk into appropriate bin */
   if (chunk && is_valid_chunk_addr(chunk))
   {
      insert_into_bin(chunk);
   }
}