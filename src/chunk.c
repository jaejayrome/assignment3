#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include "chunk.h"

/* Basic chunk operations */
int chunk_get_status(Chunk_T c) {
    assert(c != NULL);
    return c->status;
}

void chunk_set_status(Chunk_T c, int status) {
    assert(c != NULL);
    c->status = status;
}

int chunk_get_units(Chunk_T c) {
    assert(c != NULL);
    return c->units;
}

void chunk_set_units(Chunk_T c, int units) {
    assert(c != NULL);
    c->units = units;
}

Chunk_T chunk_get_next_free_chunk(Chunk_T c) {
    assert(c != NULL);
    return c->next;
}

void chunk_set_next_free_chunk(Chunk_T c, Chunk_T next) {
    assert(c != NULL);
    c->next = next;
}

/* Get footer location */
Footer_T chunk_get_footer(Chunk_T c) {
    assert(c != NULL);
    assert(c->units >= 0);
    return (Footer_T)((char *)c + (c->units * CHUNK_UNIT) + CHUNK_UNIT);
}

/* Set footer */
void chunk_set_footer(Chunk_T c) {
    assert(c != NULL);
    assert(c->units >= 0);
    
    Footer_T footer = chunk_get_footer(c);
    footer->header = c;
}

/* Get previous chunk from footer */
Chunk_T chunk_get_prev_from_footer(void *ptr, void *start) {
    Footer_T prev_footer;
    if (ptr <= start) return NULL;
    
    prev_footer = (Footer_T)((char *)ptr - sizeof(struct ChunkFooter));
    if ((void *)prev_footer < start) return NULL;
    
    return prev_footer->header;
}

/* Validate chunk */
int chunk_is_valid(Chunk_T c, void *start, void *end) {
    if (!start || !end || start >= end) return 0;
    if (!c || (void *)c < start || (void *)c >= end) return 0;
    if (c->units <= 0) return 0;
    
    /* Calculate chunk boundaries */
    size_t total_size = (c->units * CHUNK_UNIT) + CHUNK_UNIT + sizeof(struct ChunkFooter);
    void *chunk_end = (void *)((char *)c + total_size);
    if (chunk_end > end) return 0;
    
    /* Validate footer */
    Footer_T footer = chunk_get_footer(c);
    if (!footer || (void *)footer >= end) return 0;
    if (footer->header != c) return 0;
    
    return 1;
}

/* Get next adjacent chunk */
Chunk_T chunk_get_next_adjacent(Chunk_T c, void *start, void *end) {
    assert(c != NULL);
    assert((void *)c >= start);
    assert(start < end);
    
    /* Calculate next chunk location */
    size_t chunk_size = (c->units * CHUNK_UNIT) + CHUNK_UNIT + sizeof(struct ChunkFooter);
    Chunk_T next = (Chunk_T)((char *)c + chunk_size);
    
    /* Basic boundary check */
    if ((void *)next >= end) return NULL;
    
    /* Calculate next chunk boundaries */
    size_t next_total_size;
    if (next->units <= 0) return NULL;
    next_total_size = (next->units * CHUNK_UNIT) + CHUNK_UNIT + sizeof(struct ChunkFooter);
    if ((void *)((char *)next + next_total_size) > end) return NULL;
    
    /* Validate the chunk */
    Footer_T next_footer = chunk_get_footer(next);
    if (!next_footer || next_footer->header != next) return NULL;
    
    return next;
}