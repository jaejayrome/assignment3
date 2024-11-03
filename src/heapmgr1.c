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
#define MIN_SPLIT_UNITS (2 + FOOTER_UNITS)
#define TOTAL_CHUNK_SIZE(units) ((units + 1) * CHUNK_UNIT + FOOTER_SIZE)

static Chunk_T g_free_head = NULL;
static void *g_heap_start = NULL, *g_heap_end = NULL;

enum
{
    MEMALLOC_MIN = 1024
};

/* Helper functions */
static size_t size_to_units(size_t size)
{
    return (size + (CHUNK_UNIT - 1)) / CHUNK_UNIT;
}

static Chunk_T get_chunk_from_data_ptr(void *m)
{
    return (Chunk_T)((char *)m - CHUNK_UNIT);
}

static void init_my_heap(void)
{
    g_heap_start = g_heap_end = sbrk(0);
    if (g_heap_start == (void *)-1)
    {
        fprintf(stderr, "sbrk(0) failed\n");
        exit(-1);
    }
}

static int is_valid_free_chunk(Chunk_T c)
{
    if (!c || (void *)c < g_heap_start || (void *)c >= g_heap_end)
        return 0;
    return (c->units > 0);
}

/* Safe chunk validation */
static int is_valid_chunk(Chunk_T c)
{
    if (!c || (void *)c < g_heap_start || (void *)c >= g_heap_end)
        return 0;

    size_t total_size = TOTAL_CHUNK_SIZE(c->units);
    void *chunk_end = (void *)((char *)c + total_size);
    if (chunk_end > g_heap_end)
        return 0;

    Footer_T footer = chunk_get_footer(c);
    if (!footer || footer->header != c)
        return 0;

    return 1;
}

/* Safe merge function */
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2)
{
    if (!c1 || !c2 || (void *)c2 <= (void *)c1)
        return c1;

    size_t total_units = chunk_get_units(c1) + chunk_get_units(c2) + 1 + FOOTER_UNITS;
    chunk_set_units(c1, total_units - 1 - FOOTER_UNITS);
    chunk_set_next_free_chunk(c1, chunk_get_next_free_chunk(c2));
    chunk_set_footer(c1);
    return c1;
}

/* Safe split function */
static Chunk_T split_chunk(Chunk_T c, size_t units)
{
    if (!is_valid_chunk(c))
        return c;

    size_t total_units = chunk_get_units(c);
    size_t remaining = total_units - units - 1 - FOOTER_UNITS;
    if (remaining < MIN_SPLIT_UNITS)
        return c;

    /* Validate that split won't exceed heap bounds */
    void *split_chunk = (void *)((char *)c + TOTAL_CHUNK_SIZE(remaining));
    if (split_chunk >= g_heap_end)
        return c;

    chunk_set_units(c, remaining);
    chunk_set_footer(c);

    Chunk_T c2 = (Chunk_T)split_chunk;
    chunk_set_units(c2, units);
    chunk_set_status(c2, CHUNK_IN_USE);
    chunk_set_next_free_chunk(c2, NULL);
    chunk_set_footer(c2);

    return c2;
}

/* Fast insert with safety checks */
static void insert_chunk(Chunk_T c)
{
    chunk_set_status(c, CHUNK_FREE);
    chunk_set_footer(c);
    chunk_set_next_free_chunk(c, g_free_head);
    g_free_head = c;

    /* Attempt merge only if next is free head */
    Chunk_T next = chunk_get_next_adjacent(c, g_heap_start, g_heap_end);
    if (next && next == g_free_head && chunk_get_status(next) == CHUNK_FREE)
    {
        g_free_head = merge_chunk(c, next);
    }
}

/* Safe memory allocation */
static Chunk_T allocate_more_memory(size_t units)
{
    size_t alloc_units = (units < MEMALLOC_MIN) ? MEMALLOC_MIN : units;
    size_t total_size = TOTAL_CHUNK_SIZE(alloc_units);

    Chunk_T c = (Chunk_T)sbrk(total_size);
    if (c == (Chunk_T)-1)
        return NULL;

    g_heap_end = sbrk(0);

    chunk_set_units(c, alloc_units);
    chunk_set_status(c, CHUNK_FREE);
    chunk_set_next_free_chunk(c, NULL);
    chunk_set_footer(c);

    if (!is_valid_chunk(c))
        return NULL;

    insert_chunk(c);
    return c;
}

static void remove_chunk_from_list(Chunk_T prev, Chunk_T c)
{
    if (!c || !is_valid_chunk(c))
        return;

    if (prev)
        chunk_set_next_free_chunk(prev, chunk_get_next_free_chunk(c));
    else
        g_free_head = chunk_get_next_free_chunk(c);

    chunk_set_next_free_chunk(c, NULL);
    chunk_set_status(c, CHUNK_IN_USE);
    chunk_set_footer(c);
}

/* Malloc implementation */
void *heapmgr_malloc(size_t size)
{
    if (size <= 0)
        return NULL;

    static int is_init = FALSE;
    if (!is_init)
    {
        init_my_heap();
        is_init = TRUE;
    }

    size_t units = size_to_units(size);
    Chunk_T prev = NULL;
    Chunk_T curr = g_free_head;

    while (curr && is_valid_chunk(curr))
    {
        if (chunk_get_units(curr) >= units)
        {
            if (chunk_get_units(curr) > units + MIN_SPLIT_UNITS)
                curr = split_chunk(curr, units);
            remove_chunk_from_list(prev, curr);
            return (void *)((char *)curr + CHUNK_UNIT);
        }
        prev = curr;
        curr = chunk_get_next_free_chunk(curr);
    }

    curr = allocate_more_memory(units);
    if (!curr)
        return NULL;

    if (chunk_get_units(curr) > units + MIN_SPLIT_UNITS)
        curr = split_chunk(curr, units);
    remove_chunk_from_list(NULL, curr);

    return (void *)((char *)curr + CHUNK_UNIT);
}

/* Fast and safe free */
void heapmgr_free(void *m)
{
    if (!m)
        return;

    Chunk_T c = get_chunk_from_data_ptr(m);
    /* Only check heap bounds and chunk is in use */
    if ((void *)c < g_heap_start || (void *)c >= g_heap_end ||
        chunk_get_status(c) != CHUNK_IN_USE)
        return;

    insert_chunk(c);
}