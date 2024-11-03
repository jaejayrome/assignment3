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

/* Bin configuration */
#define NUM_BINS 32
#define MIN_ALLOC_UNITS (64 / CHUNK_UNIT)  // 64 bytes minimum

/* Static variables */
static Chunk_T g_bins[NUM_BINS];
static void *g_heap_start = NULL, *g_heap_end = NULL;

/* Helper functions */
static int get_bin_index(size_t size) {
    size_t units = (size + CHUNK_UNIT - 1) / CHUNK_UNIT;
    
    // Fast path for small allocations
    if (units <= 4) return 0;      // 0-64 bytes
    if (units <= 8) return 1;      // 65-128 bytes
    if (units <= 16) return 2;     // 129-256 bytes
    if (units <= 32) return 3;     // 257-512 bytes
    
    // Use bit manipulation for larger sizes
    unsigned int index = 4;
    size_t adjusted_size = units - 1;
    while (adjusted_size >>= 1) index++;
    return (index < NUM_BINS - 1) ? index : NUM_BINS - 1;
}

static size_t size_to_units(size_t size) {
    return (size + CHUNK_UNIT - 1) / CHUNK_UNIT;
}

static void init_my_heap(void) {
    g_heap_start = g_heap_end = sbrk(0);
    if (g_heap_start == (void *)-1) {
        fprintf(stderr, "sbrk(0) failed\n");
        exit(-1);
    }
    
    // Initialize bins
    for (int i = 0; i < NUM_BINS; i++) {
        g_bins[i] = NULL;
    }
}

/* Safe chunk validation */
static int is_valid_chunk(Chunk_T c) {
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
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2) {
    if (!c1 || !c2 || (void *)c2 <= (void *)c1)
        return c1;

    // Verify chunks are adjacent
    size_t c1_size = TOTAL_CHUNK_SIZE(chunk_get_units(c1));
    if ((char *)c1 + c1_size != (char *)c2)
        return c1;

    // Both chunks must be free
    if (chunk_get_status(c1) != CHUNK_FREE || chunk_get_status(c2) != CHUNK_FREE)
        return c1;

    size_t total_units = chunk_get_units(c1) + chunk_get_units(c2) + 1 + FOOTER_UNITS;
    chunk_set_units(c1, total_units - 1 - FOOTER_UNITS);
    
    // Update doubly-linked list pointers
    if (c2->next) c2->next->prev = c2->prev;
    if (c2->prev) c2->prev->next = c2->next;
    
    chunk_set_footer(c1);
    return c1;
}

/* Insert chunk into appropriate bin */
static void insert_into_bin(Chunk_T chunk) {
    if (!chunk || !is_valid_chunk(chunk))
        return;

    int bin_index = get_bin_index(chunk_get_units(chunk) * CHUNK_UNIT);
    
    // Set chunk status before linking
    chunk_set_status(chunk, CHUNK_FREE);
    chunk_set_footer(chunk);

    // Insert at head of bin
    chunk->prev = NULL;
    chunk->next = g_bins[bin_index];
    if (g_bins[bin_index])
        g_bins[bin_index]->prev = chunk;
    g_bins[bin_index] = chunk;
}

/* Remove chunk from its bin */
static void remove_from_bin(Chunk_T chunk) {
    if (!chunk || !is_valid_chunk(chunk))
        return;

    // Update adjacent nodes
    if (chunk->prev)
        chunk->prev->next = chunk->next;
    else {
        int bin_index = get_bin_index(chunk_get_units(chunk) * CHUNK_UNIT);
        g_bins[bin_index] = chunk->next;
    }

    if (chunk->next)
        chunk->next->prev = chunk->prev;

    // Clear chunk links and update status
    chunk->prev = chunk->next = NULL;
    chunk_set_status(chunk, CHUNK_IN_USE);
    chunk_set_footer(chunk);
}

/* Split chunk if needed */
static Chunk_T split_chunk(Chunk_T c, size_t units) {
    if (!is_valid_chunk(c))
        return c;

    size_t total_units = chunk_get_units(c);
    size_t remaining = total_units - units - 1 - FOOTER_UNITS;
    
    if (remaining < MIN_SPLIT_UNITS)
        return c;

    void *split_pos = (char *)c + TOTAL_CHUNK_SIZE(units);
    if (split_pos >= g_heap_end)
        return c;

    // Create new chunk
    Chunk_T new_chunk = (Chunk_T)split_pos;
    chunk_set_units(new_chunk, remaining);
    chunk_set_status(new_chunk, CHUNK_FREE);
    chunk_set_footer(new_chunk);

    // Update original chunk
    chunk_set_units(c, units);
    chunk_set_footer(c);

    // Insert remainder into appropriate bin
    insert_into_bin(new_chunk);

    return c;
}

/* Find suitable chunk */
static Chunk_T find_chunk(size_t size) {
    size_t required_units = size_to_units(size);
    int bin_index = get_bin_index(size);
    
    // Search in current and larger bins
    for (int i = bin_index; i < NUM_BINS; i++) {
        for (Chunk_T chunk = g_bins[i]; chunk != NULL; chunk = chunk->next) {
            if (is_valid_chunk(chunk) && chunk_get_units(chunk) >= required_units) {
                remove_from_bin(chunk);
                return chunk;
            }
        }
    }
    
    return NULL;
}

/* Malloc implementation */
void *heapmgr_malloc(size_t size) {
    if (size == 0) return NULL;

    static int is_init = FALSE;
    if (!is_init) {
        init_my_heap();
        is_init = TRUE;
    }

    size_t units = size_to_units(size);
    if (units < MIN_ALLOC_UNITS) units = MIN_ALLOC_UNITS;

    // Try to find a suitable chunk
    Chunk_T chunk = find_chunk(size);
    if (chunk) {
        if (chunk_get_units(chunk) > units + MIN_SPLIT_UNITS)
            chunk = split_chunk(chunk, units);
        return (void *)((char *)chunk + CHUNK_UNIT);
    }

    // Allocate new memory
    size_t alloc_units = (units <= 256/CHUNK_UNIT) ? 256/CHUNK_UNIT : units;
    chunk = sbrk(TOTAL_CHUNK_SIZE(alloc_units));
    if (chunk == (void *)-1) return NULL;
    
    g_heap_end = sbrk(0);
    
    chunk_set_units(chunk, alloc_units);
    chunk_set_status(chunk, CHUNK_IN_USE);
    chunk_set_footer(chunk);

    // Split if possible
    if (alloc_units > units + MIN_SPLIT_UNITS) {
        chunk = split_chunk(chunk, units);
    }

    return (void *)((char *)chunk + CHUNK_UNIT);
}

/* Free implementation */
void heapmgr_free(void *ptr) {
    if (!ptr) return;

    Chunk_T chunk = (Chunk_T)((char *)ptr - CHUNK_UNIT);
    if (!is_valid_chunk(chunk) || chunk_get_status(chunk) != CHUNK_IN_USE)
        return;

    // Get adjacent chunks
    Chunk_T prev = NULL;
    Chunk_T next = NULL;

    // Find previous chunk using footer
    void *prev_footer = (char *)chunk - sizeof(struct ChunkFooter);
    if (prev_footer >= g_heap_start) {
        Footer_T footer = (Footer_T)prev_footer;
        prev = footer->header;
        if (!is_valid_chunk(prev)) prev = NULL;
    }

    // Find next chunk
    void *next_addr = (char *)chunk + TOTAL_CHUNK_SIZE(chunk_get_units(chunk));
    if (next_addr < g_heap_end) {
        next = (Chunk_T)next_addr;
        if (!is_valid_chunk(next)) next = NULL;
    }

    // Insert chunk into appropriate bin
    insert_into_bin(chunk);

    // Merge with neighbors if possible
    if (prev && chunk_get_status(prev) == CHUNK_FREE) {
        remove_from_bin(prev);
        remove_from_bin(chunk);
        chunk = merge_chunk(prev, chunk);
        insert_into_bin(chunk);
    }

    if (next && chunk_get_status(next) == CHUNK_FREE) {
        remove_from_bin(next);
        remove_from_bin(chunk);
        chunk = merge_chunk(chunk, next);
        insert_into_bin(chunk);
    }
}