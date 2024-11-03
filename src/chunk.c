#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include "chunk.h"

/* Basic operations remain the same */
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

static int is_aligned(void *ptr)
{
    size_t addr = (size_t)ptr;
    return (addr % CHUNK_UNIT) == 0;
}

/* Safe footer calculation */
static size_t get_chunk_size(int units)
{
    return (size_t)((units * CHUNK_UNIT) + CHUNK_UNIT);
}

static size_t get_total_size(int units)
{
    return get_chunk_size(units) + sizeof(struct ChunkFooter);
}

Footer_T chunk_get_footer(Chunk_T c)
{
    assert(c != NULL);
    assert(c->units >= 0);
    return (Footer_T)((char *)c + (c->units * CHUNK_UNIT) + CHUNK_UNIT);
}

void chunk_set_footer(Chunk_T c)
{
    assert(c != NULL);
    assert(c->units >= 0);
    Footer_T footer = chunk_get_footer(c);
    footer->header = c;
}

Chunk_T chunk_get_prev_from_footer(void *ptr, void *start)
{
    if (!ptr || !start || ptr <= start)
        return NULL;

    Footer_T prev_footer = (Footer_T)((char *)ptr - sizeof(struct ChunkFooter));
    if ((void *)prev_footer < start)
        return NULL;
    if (!prev_footer->header)
        return NULL;

    Chunk_T prev = prev_footer->header;
    if ((void *)prev < start)
        return NULL;

    return prev;
}

/* Safe validation function */
int chunk_is_valid(Chunk_T c, void *start, void *end)
{
    /* Basic pointer validation */
    if (!c || !start || !end || start >= end)
        return 0;
    if ((void *)c < start || (void *)c >= end)
        return 0;
    if (!is_aligned((void *)c))
        return 0;

    /* Check units */
    if (c->units <= 0)
        return 0;

    /* Calculate and check boundaries */
    size_t chunk_size = (c->units * CHUNK_UNIT) + CHUNK_UNIT;
    if (chunk_size < sizeof(struct Chunk))
        return 0;

    void *chunk_end = (void *)((char *)c + chunk_size + sizeof(struct ChunkFooter));
    if (chunk_end > end)
        return 0;

    /* Check footer existence */
    void *footer_addr = (void *)((char *)c + chunk_size);
    if (footer_addr >= end)
        return 0;

    Footer_T footer = (Footer_T)footer_addr;
    if (!footer)
        return 0;

    /* Verify back reference only if we can safely access it */
    if ((void *)footer < end &&
        (void *)((char *)footer + sizeof(struct ChunkFooter)) <= end)
    {
        if (footer->header != c)
            return 0;
    }

    return 1;
}

/* Safe adjacency check */
Chunk_T chunk_get_next_adjacent(Chunk_T c, void *start, void *end)
{
    /* Basic validation */
    if (!c || !start || !end || start >= end)
        return NULL;
    if ((void *)c < start || (void *)c >= end)
        return NULL;
    if (c->units <= 0)
        return NULL;

    /* Calculate next chunk position */
    size_t size = (c->units * CHUNK_UNIT) + CHUNK_UNIT + sizeof(struct ChunkFooter);
    void *next_pos = (void *)((char *)c + size);

    /* Basic boundary check */
    if (next_pos >= end ||
        (void *)((char *)next_pos + sizeof(struct Chunk)) > end)
    {
        return NULL;
    }

    Chunk_T next = (Chunk_T)next_pos;

    /* Only proceed if address is properly aligned */
    if (!is_aligned((void *)next))
        return NULL;

    /* Basic structure validation before dereferencing */
    if (next->units <= 0)
        return NULL;

    /* Full validation */
    if (!chunk_is_valid(next, start, end))
        return NULL;

    return next;
}