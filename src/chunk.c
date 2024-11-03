#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include "chunk.h"

/* Basic operations remain the same */
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

/* Safe footer calculation */
static size_t get_chunk_size(int units) {
    return (size_t)((units * CHUNK_UNIT) + CHUNK_UNIT);
}

static size_t get_total_size(int units) {
    return get_chunk_size(units) + sizeof(struct ChunkFooter);
}

Footer_T chunk_get_footer(Chunk_T c) {
    assert(c != NULL);
    assert(c->units >= 0);
    return (Footer_T)((char *)c + get_chunk_size(c->units));
}

void chunk_set_footer(Chunk_T c) {
    assert(c != NULL);
    assert(c->units >= 0);
    Footer_T footer = chunk_get_footer(c);
    if (footer) {
        footer->header = c;
    }
}

Chunk_T chunk_get_prev_from_footer(void *ptr, void *start) {
    if (!ptr || !start || ptr <= start) return NULL;
    
    void *footer_addr = (void *)((char *)ptr - sizeof(struct ChunkFooter));
    if (footer_addr < start) return NULL;
    
    Footer_T prev_footer = (Footer_T)footer_addr;
    if (!prev_footer || !prev_footer->header) return NULL;
    
    Chunk_T prev = prev_footer->header;
    if ((void *)prev < start) return NULL;
    
    /* Validate that this footer actually belongs to the previous chunk */
    Footer_T check_footer = chunk_get_footer(prev);
    if (check_footer != prev_footer) return NULL;
    
    return prev;
}

/* Safe validation function */
int chunk_is_valid(Chunk_T c, void *start, void *end) {
    /* Basic pointer checks */
    if (!c || !start || !end || start >= end) return 0;
    if ((void *)c < start || (void *)c >= end) return 0;
    
    /* Validate basic structure */
    if (c->units <= 0) return 0;
    
    /* Validate chunk boundaries */
    size_t chunk_size = get_chunk_size(c->units);
    void *footer_pos = (void *)((char *)c + chunk_size);
    if (footer_pos >= end) return 0;
    
    /* Check if footer would exceed boundary */
    if ((void *)((char *)footer_pos + sizeof(struct ChunkFooter)) > end) return 0;
    
    /* Now safe to check footer */
    Footer_T footer = (Footer_T)footer_pos;
    if (!footer) return 0;
    
    /* Verify footer points back to this chunk */
    if (footer->header != c) return 0;
    
    return 1;
}

/* Safe adjacency check */
Chunk_T chunk_get_next_adjacent(Chunk_T c, void *start, void *end) {
    /* Basic pointer validation */
    if (!c || !start || !end || start >= end) return NULL;
    if ((void *)c < start || (void *)c >= end) return NULL;
    
    /* Calculate next chunk position */
    size_t total_size = get_total_size(c->units);
    void *next_addr = (void *)((char *)c + total_size);
    
    /* Verify next chunk would fit */
    if (next_addr >= end) return NULL;
    if ((void *)((char *)next_addr + sizeof(struct Chunk)) > end) return NULL;
    
    Chunk_T next = (Chunk_T)next_addr;
    
    /* Extra validation before returning */
    if (!chunk_is_valid(next, start, end)) return NULL;
    
    return next;
}