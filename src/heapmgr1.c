/*--------------------------------------------------------------------*/
/* heapmgr1.c */
/*--------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "chunk.h"
#include "heapmgr.h"

#define FALSE 0
#define TRUE 1

/* Size calculation macros */
#define FOOTER_SIZE (sizeof(struct ChunkFooter))
#define FOOTER_UNITS ((FOOTER_SIZE + CHUNK_UNIT - 1) / CHUNK_UNIT)
#define MIN_SPLIT_UNITS (2 + FOOTER_UNITS) // Minimum units needed for a split

static Chunk_T g_free_head = NULL;
static void *g_heap_start = NULL, *g_heap_end = NULL;

/* Helper function declarations */
static size_t size_to_units(size_t size);
static Chunk_T get_chunk_from_data_ptr(void *m);
static void init_my_heap(void);

enum
{
    MEMALLOC_MIN = 1024 /* Minimum allocation size */
};

/* Helper function implementation */
static size_t size_to_units(size_t size)
{
    return (size + (CHUNK_UNIT - 1)) / CHUNK_UNIT;
}

static Chunk_T get_chunk_from_data_ptr(void *m)
{
    return (Chunk_T)((char *)m - CHUNK_UNIT);
}

static size_t calculate_chunk_size(size_t units)
{
    return (units * CHUNK_UNIT) + CHUNK_UNIT + sizeof(struct ChunkFooter);
}

static int is_valid_chunk_addr(void *addr)
{
    return (addr >= g_heap_start && addr < g_heap_end);
}

static int is_valid_footer_addr(void *footer_addr, Chunk_T c)
{
    /* Check if footer location makes sense */
    return (footer_addr > (void *)c &&
            footer_addr >= g_heap_start &&
            footer_addr < g_heap_end);
}

/* Merge two adjacent chunks */
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2)
{
    if (!c1 || !c2 || c1 >= c2 ||
        !is_valid_chunk_addr(c1) || !is_valid_chunk_addr(c2))
    {
        return c1;
    }

    /* Calculate total size after merge */
    size_t total_units = chunk_get_units(c1) + chunk_get_units(c2) + 1 + FOOTER_UNITS;
    void *merged_end = (void *)((char *)c1 + (total_units * CHUNK_UNIT) + sizeof(struct ChunkFooter));

    if (merged_end > g_heap_end)
    {
        return c1;
    }

    if (chunk_get_status(c1) != CHUNK_FREE || chunk_get_status(c2) != CHUNK_FREE)
    {
        return c1;
    }

    /* Perform merge */
    chunk_set_units(c1, total_units - 1 - FOOTER_UNITS);
    chunk_set_next_free_chunk(c1, chunk_get_next_free_chunk(c2));
    chunk_set_footer(c1);

    return c1;
}
/* Insert chunk into free list */
static void insert_chunk(Chunk_T c)
{
    assert(c != NULL);
    assert(chunk_get_units(c) >= 1);

    /* Initialize the chunk */
    chunk_set_status(c, CHUNK_FREE);
    chunk_set_footer(c);

    if (g_free_head == NULL)
    {
        /* First chunk in list */
        g_free_head = c;
        chunk_set_next_free_chunk(c, NULL);
    }
    else
    {
        /* Insert at head and check for merge */
        assert(c < g_free_head);
        chunk_set_next_free_chunk(c, g_free_head);

        if (chunk_get_next_adjacent(c, g_heap_start, g_heap_end) == g_free_head)
        {
            c = merge_chunk(c, g_free_head);
        }
        g_free_head = c;
    }
}

/* Remove chunk from free list */
static void remove_chunk_from_list(Chunk_T prev, Chunk_T c)
{
    if (!c)
        return;

    assert(chunk_get_status(c) == CHUNK_FREE);

    if (prev == NULL)
    {
        g_free_head = chunk_get_next_free_chunk(c);
    }
    else
    {
        chunk_set_next_free_chunk(prev, chunk_get_next_free_chunk(c));
    }

    chunk_set_next_free_chunk(c, NULL);
    chunk_set_status(c, CHUNK_IN_USE);
    chunk_set_footer(c);
}

/* Split chunk into two parts */
static Chunk_T split_chunk(Chunk_T c, size_t units)
{
    Chunk_T c2;
    size_t all_units;

    assert(chunk_is_valid(c, g_heap_start, g_heap_end));
    assert(chunk_get_status(c) == CHUNK_FREE);
    assert(chunk_get_units(c) > units + MIN_SPLIT_UNITS);

    all_units = chunk_get_units(c);
    chunk_set_units(c, all_units - units - 1 - FOOTER_UNITS);
    chunk_set_footer(c);

    c2 = chunk_get_next_adjacent(c, g_heap_start, g_heap_end);
    assert(chunk_is_valid(c2, g_heap_start, g_heap_end));

    chunk_set_units(c2, units);
    chunk_set_status(c2, CHUNK_IN_USE);
    chunk_set_next_free_chunk(c2, NULL);
    chunk_set_footer(c2);

    return c2;
}

/* Insert chunk after a specific chunk */
static Chunk_T insert_chunk_after(Chunk_T new_chunk, Chunk_T prev_chunk)
{
    /* Basic pointer checks */
    if (!new_chunk || !prev_chunk)
    {
        return NULL;
    }

    /* Validate both chunks */
    if (!chunk_is_valid(new_chunk, g_heap_start, g_heap_end) ||
        !chunk_is_valid(prev_chunk, g_heap_start, g_heap_end))
    {
        return NULL;
    }

    /* Check status of new chunk */
    if (chunk_get_status(new_chunk) != CHUNK_FREE)
    {
        return NULL;
    }

    /* Initialize the chunk */
    chunk_set_status(prev_chunk, CHUNK_FREE);
    chunk_set_footer(prev_chunk);

    /* Link chunks */
    chunk_set_next_free_chunk(prev_chunk, chunk_get_next_free_chunk(new_chunk));
    chunk_set_next_free_chunk(new_chunk, prev_chunk);

    /* Try to merge with neighbors */
    Chunk_T next = chunk_get_next_adjacent(new_chunk, g_heap_start, g_heap_end);
    if (next == prev_chunk)
    {
        return merge_chunk(new_chunk, prev_chunk);
    }

    next = chunk_get_next_adjacent(prev_chunk, g_heap_start, g_heap_end);
    if (next && chunk_get_status(next) == CHUNK_FREE)
    {
        return merge_chunk(prev_chunk, next);
    }

    return prev_chunk;
}
/* Initialize heap */
static void init_my_heap(void)
{
    g_heap_start = g_heap_end = sbrk(0);
    if (g_heap_start == (void *)-1)
    {
        fprintf(stderr, "sbrk(0) failed\n");
        exit(-1);
    }
}

/* Allocate more memory */
static Chunk_T allocate_more_memory(Chunk_T prev, size_t units)
{
    Chunk_T c;
    size_t total_size;

    if (units < MEMALLOC_MIN)
    {
        units = MEMALLOC_MIN;
    }

    /* Calculate size including header and footer */
    total_size = ((units + 1) * CHUNK_UNIT + sizeof(struct ChunkFooter) +
                  (CHUNK_UNIT - 1)) &
                 ~(CHUNK_UNIT - 1);

    /* Allocate memory */
    c = (Chunk_T)sbrk(total_size);
    if (c == (Chunk_T)-1)
    {
        return NULL;
    }

    /* Update heap end */
    g_heap_end = sbrk(0);

    /* Initialize new chunk */
    chunk_set_units(c, units);
    chunk_set_next_free_chunk(c, NULL);
    chunk_set_status(c, CHUNK_FREE);
    chunk_set_footer(c);

    /* Handle insertion */
    if (g_free_head == NULL || prev == NULL)
    {
        /* Insert at head */
        chunk_set_next_free_chunk(c, g_free_head);
        g_free_head = c;

        /* Try to merge if possible */
        if (g_free_head->next &&
            chunk_get_next_adjacent(c, g_heap_start, g_heap_end) == g_free_head->next)
        {
            c = merge_chunk(c, g_free_head->next);
        }
        return c;
    }

    /* Try to insert after prev - NOTE THE CORRECTED PARAMETER ORDER */
    if (chunk_is_valid(prev, g_heap_start, g_heap_end))
    {
        Chunk_T result = insert_chunk_after(c, prev); // Fixed parameter order
        if (result)
        {
            return result;
        }
    }

    /* If all else fails, insert at head */
    chunk_set_next_free_chunk(c, g_free_head);
    g_free_head = c;
    return c;
}

/* Malloc implementation */
void *heapmgr_malloc(size_t size)
{
    static int is_init = FALSE;
    Chunk_T c, prev = NULL, pprev = NULL;
    size_t units;

    if (size <= 0)
        return NULL;

    if (!is_init)
    {
        init_my_heap();
        is_init = TRUE;
    }

    units = size_to_units(size);

    /* Search free list */
    for (c = g_free_head; c != NULL; c = chunk_get_next_free_chunk(c))
    {
        if (chunk_get_units(c) >= units)
        {
            /* Found a chunk */
            if (chunk_get_units(c) > units + MIN_SPLIT_UNITS)
            {
                c = split_chunk(c, units);
            }
            else
            {
                remove_chunk_from_list(prev, c);
            }
            return (void *)((char *)c + CHUNK_UNIT);
        }
        pprev = prev;
        prev = c;
    }

    /* Need to allocate more memory */
    c = allocate_more_memory(prev, units);
    if (!c)
        return NULL;

    /* Update prev if needed */
    if (c == prev)
        prev = pprev;

    /* Check if we can split */
    if (chunk_get_units(c) > units + MIN_SPLIT_UNITS)
        c = split_chunk(c, units);
    else
        remove_chunk_from_list(prev, c);

    return (void *)((char *)c + CHUNK_UNIT);
}

/* Free implementation */
void heapmgr_free(void *m)
{
    if (m == NULL)
        return;

    Chunk_T c = get_chunk_from_data_ptr(m);

    /* Validate chunk addresses */
    if (!is_valid_chunk_addr(c))
    {
        return;
    }

    Footer_T footer = chunk_get_footer(c);
    if (!is_valid_footer_addr(footer, c))
    {
        return;
    }

    /* Validate chunk is in use */
    if (chunk_get_status(c) != CHUNK_IN_USE)
    {
        return; // Double free prevention
    }

    /* Set up basic chunk info */
    chunk_set_status(c, CHUNK_FREE);
    chunk_set_footer(c);

    /* Get neighbors */
    Chunk_T prev = NULL;
    Chunk_T next = NULL;

    /* Try to get prev chunk if we're not at start */
    if ((void *)c > g_heap_start)
    {
        void *prev_footer_addr = (void *)c - sizeof(struct ChunkFooter);
        if (is_valid_chunk_addr(prev_footer_addr))
        {
            prev = chunk_get_prev_from_footer((void *)c, g_heap_start);
            if (!is_valid_chunk_addr(prev))
            {
                prev = NULL;
            }
        }
    }

    /* Try to get next chunk */
    next = (void *)((char *)footer + sizeof(struct ChunkFooter));
    if (!is_valid_chunk_addr(next))
    {
        next = NULL;
    }

    /* If no valid prev chunk or prev isn't free, add to free list head */
    if (!prev || chunk_get_status(prev) != CHUNK_FREE)
    {
        chunk_set_next_free_chunk(c, g_free_head);
        g_free_head = c;
    }

    /* Try to merge with prev if possible */
    if (prev && chunk_get_status(prev) == CHUNK_FREE)
    {
        /* Validate merged chunk size before merging */
        size_t total_units = chunk_get_units(prev) + chunk_get_units(c) + 1 + FOOTER_UNITS;
        void *merged_end = (void *)((char *)prev + (total_units * CHUNK_UNIT) + sizeof(struct ChunkFooter));

        if (merged_end <= g_heap_end)
        {
            c = merge_chunk(prev, c);
        }
    }

    /* Try to merge with next if possible */
    if (next && chunk_get_status(next) == CHUNK_FREE)
    {
        /* Validate merged chunk size before merging */
        size_t total_units = chunk_get_units(c) + chunk_get_units(next) + 1 + FOOTER_UNITS;
        void *merged_end = (void *)((char *)c + (total_units * CHUNK_UNIT) + sizeof(struct ChunkFooter));

        if (merged_end <= g_heap_end)
        {
            merge_chunk(c, next);
        }
    }
}