#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include "chunk.h"

/* Basic chunk operations - no heap knowledge needed */
int chunk_get_status(Chunk_T c)
{
    assert(c != NULL);
    return c->status;
}

void chunk_set_status(Chunk_T c, int status)
{
    assert(c != NULL);
    c->status = status;
}

int chunk_get_units(Chunk_T c)
{
    assert(c != NULL);
    return c->units;
}

void chunk_set_units(Chunk_T c, int units)
{
    assert(c != NULL);
    c->units = units;
}

Chunk_T chunk_get_next_free_chunk(Chunk_T c)
{
    assert(c != NULL);
    return c->next;
}

void chunk_set_next_free_chunk(Chunk_T c, Chunk_T next)
{
    assert(c != NULL);
    c->next = next;
}

/* Functions that need heap bounds */
Chunk_T chunk_get_next_adjacent(Chunk_T c, void *start, void *end)
{
    Chunk_T next;
    size_t chunk_total_size;

    assert(c != NULL);
    assert((void *)c >= start);

    /* Calculate total size including header, data, and footer */
    chunk_total_size = (c->units * CHUNK_UNIT) + CHUNK_UNIT + sizeof(struct ChunkFooter);
    next = (Chunk_T)((char *)c + chunk_total_size);

    if ((void *)next >= end)
    {
        return NULL;
    }

    return next;
}

Footer_T chunk_get_footer(Chunk_T c)
{
    assert(c != NULL);
    assert(c->units >= 0);

    /* Calculate footer address */
    return (Footer_T)((char *)c + (c->units * CHUNK_UNIT) + CHUNK_UNIT);
}

void chunk_set_footer(Chunk_T c)
{
    Footer_T footer;
    assert(c != NULL);
    assert(c->units >= 0);

    footer = chunk_get_footer(c);
    if (footer != NULL)
    {
        footer->header = c;
    }
}

Chunk_T chunk_get_prev_from_footer(void *ptr, void *start)
{
    Footer_T prev_footer;

    if (ptr <= start)
        return NULL;

    prev_footer = (Footer_T)((char *)ptr - sizeof(struct ChunkFooter));
    return prev_footer->header;
}

int chunk_is_valid(Chunk_T c, void *start, void *end)
{
    assert(c != NULL);
    assert(start != NULL);
    assert(end != NULL);

    if (c < (Chunk_T)start)
        return 0;
    if (c >= (Chunk_T)end)
        return 0;
    if (c->units == 0)
        return 0;
    return 1;
}
