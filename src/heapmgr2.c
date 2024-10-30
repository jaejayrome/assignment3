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

/* Bin configuration - simplified for better performance */
#define NUM_BINS 32
#define TOTAL_BINS NUM_BINS
#define MIN_ALLOC_UNITS (64 / CHUNK_UNIT)  // 64 bytes minimum

/* Static variables */
static Chunk_T g_bins[NUM_BINS];
static void *g_heap_start = NULL, *g_heap_end = NULL;

/* Forward declarations */
static int get_bin_index(size_t size);
static void init_bins(void);
static void insert_into_bin(Chunk_T chunk);
static void remove_from_bin(Chunk_T chunk);
static Chunk_T find_chunk(size_t size);
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2);

/* Fast bin index calculation */
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

/* Validation helpers */
static int is_valid_chunk_addr(void *addr) {
    return (addr >= g_heap_start && addr < g_heap_end);
}

static int is_valid_footer_addr(void *footer_addr, Chunk_T c) {
    return (footer_addr > (void *)c &&
            footer_addr >= g_heap_start &&
            footer_addr < g_heap_end &&
            ((size_t)((char *)footer_addr - (char *)c)) >= (CHUNK_UNIT + sizeof(struct ChunkFooter)));
}

/* Initialize bins */
static void init_bins() {
    for (int i = 0; i < NUM_BINS; i++) {
        g_bins[i] = NULL;
    }
}

/* Insert chunk into appropriate bin */
static void insert_into_bin(Chunk_T chunk) {
    if (!chunk || !is_valid_chunk_addr(chunk)) return;

    size_t units = chunk_get_units(chunk);
    if (units <= 0 || units > ((size_t)((char *)g_heap_end - (char *)g_heap_start) / CHUNK_UNIT)) return;

    Footer_T footer = (Footer_T)((char *)chunk + (units * CHUNK_UNIT) + CHUNK_UNIT);
    if (!is_valid_footer_addr((void *)footer, chunk)) return;

    int bin_index = get_bin_index(units * CHUNK_UNIT);
    if (bin_index < 0 || bin_index >= NUM_BINS) return;

    /* Insert at head of bin */
    chunk->prev = NULL;
    chunk->next = g_bins[bin_index];
    if (g_bins[bin_index]) {
        g_bins[bin_index]->prev = chunk;
    }
    g_bins[bin_index] = chunk;

    chunk_set_status(chunk, CHUNK_FREE);
    footer->header = chunk;
}

/* Remove chunk from its bin */
static void remove_from_bin(Chunk_T chunk) {
    if (chunk->prev) {
        chunk->prev->next = chunk->next;
    } else {
        int bin_index = get_bin_index(chunk_get_units(chunk) * CHUNK_UNIT);
        g_bins[bin_index] = chunk->next;
    }

    if (chunk->next) {
        chunk->next->prev = chunk->prev;
    }

    chunk->prev = chunk->next = NULL;
    chunk_set_status(chunk, CHUNK_IN_USE);
    chunk_set_footer(chunk);
}

/* Find suitable chunk */
static Chunk_T find_chunk(size_t size) {
    int bin_index = get_bin_index(size);
    size_t required_units = (size + CHUNK_UNIT - 1) / CHUNK_UNIT;
    
    /* Look in bins */
    for (int i = bin_index; i < NUM_BINS; i++) {
        Chunk_T chunk = g_bins[i];
        while (chunk) {
            if (chunk_get_units(chunk) >= required_units) {
                remove_from_bin(chunk);
                return chunk;
            }
            chunk = chunk->next;
        }
        /* If this was the exact size bin and we didn't find a fit, 
           skip to significantly larger bin to avoid small splits */
        if (i == bin_index) {
            i = bin_index + 2;
        }
    }
    return NULL;
}

/* Malloc implementation */
void *heapmgr_malloc(size_t size) {
    static int is_init = FALSE;
    Chunk_T chunk;
    size_t units;

    if (size <= 0) return NULL;

    if (!is_init) {
        init_bins();
        g_heap_start = g_heap_end = sbrk(0);
        is_init = TRUE;
    }

    units = (size + CHUNK_UNIT - 1) / CHUNK_UNIT;
    if (units < MIN_ALLOC_UNITS) units = MIN_ALLOC_UNITS;

    /* Try to find suitable chunk */
    chunk = find_chunk(size);
    if (chunk) {
        /* Only split if the remainder is significant */
        if (chunk_get_units(chunk) >= units + MIN_ALLOC_UNITS * 2) {
            Chunk_T new_chunk = (Chunk_T)((char *)chunk + 
                                      (units + 1) * CHUNK_UNIT + FOOTER_SIZE);
            
            chunk_set_units(new_chunk, chunk_get_units(chunk) - units - 1 - FOOTER_UNITS);
            chunk_set_units(chunk, units);
            
            insert_into_bin(new_chunk);
        }
        return (void *)((char *)chunk + CHUNK_UNIT);
    }

    /* Allocate new memory */
    size_t alloc_units = (units <= 256/CHUNK_UNIT) ? 256/CHUNK_UNIT : units;
    chunk = sbrk((alloc_units + 1) * CHUNK_UNIT + FOOTER_SIZE);
    if (chunk == (void *)-1) return NULL;
    
    g_heap_end = sbrk(0);
    
    chunk_set_units(chunk, alloc_units);
    chunk_set_status(chunk, CHUNK_IN_USE);
    chunk_set_footer(chunk);

    /* Split if we have a significant remainder */
    if (alloc_units >= units + MIN_ALLOC_UNITS * 2) {
        Chunk_T extra = (Chunk_T)((char *)chunk + (units + 1) * CHUNK_UNIT + FOOTER_SIZE);
        chunk_set_units(extra, alloc_units - units - 1 - FOOTER_UNITS);
        insert_into_bin(extra);
        
        chunk_set_units(chunk, units);
        chunk_set_footer(chunk);
    }

    return (void *)((char *)chunk + CHUNK_UNIT);
}

/* Merge chunks */
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2) {
    if (!c1 || !c2 || !is_valid_chunk_addr(c1) || !is_valid_chunk_addr(c2) || c1 >= c2) {
        return c1;
    }

    size_t units1 = chunk_get_units(c1);
    size_t units2 = chunk_get_units(c2);

    if (units1 <= 0 || units2 <= 0) return c1;
    if ((char *)c1 + (units1 + 1) * CHUNK_UNIT + FOOTER_SIZE != (char *)c2) return c1;
    if (chunk_get_status(c1) != CHUNK_FREE || chunk_get_status(c2) != CHUNK_FREE) return c1;

    size_t total_units = units1 + units2 + 1 + FOOTER_UNITS;
    chunk_set_units(c1, total_units - 1 - FOOTER_UNITS);
    chunk_set_footer(c1);

    return c1;
}

/* Free implementation */
void heapmgr_free(void *ptr) {
    if (!ptr || !is_valid_chunk_addr(ptr)) return;

    Chunk_T chunk = (Chunk_T)((char *)ptr - CHUNK_UNIT);
    if (!is_valid_chunk_addr(chunk)) return;

    size_t chunk_units = chunk_get_units(chunk);
    if (chunk_units <= 0 || chunk_get_status(chunk) != CHUNK_IN_USE) return;

    /* Try to merge with neighbors */
    Chunk_T prev = NULL;
    Chunk_T next = NULL;

    /* Find prev chunk */
    if ((void *)chunk > g_heap_start) {
        void *prev_footer_addr = (void *)((char *)chunk - sizeof(struct ChunkFooter));
        if (is_valid_chunk_addr(prev_footer_addr)) {
            Footer_T prev_footer = (Footer_T)prev_footer_addr;
            prev = prev_footer->header;
            if (!is_valid_chunk_addr(prev) || prev >= chunk) prev = NULL;
        }
    }

    /* Find next chunk */
    void *next_addr = (void *)((char *)chunk + (chunk_units * CHUNK_UNIT) + CHUNK_UNIT + FOOTER_SIZE);
    if (is_valid_chunk_addr(next_addr)) {
        next = (Chunk_T)next_addr;
        size_t next_units = chunk_get_units(next);
        if (next_units <= 0) next = NULL;
    }

    /* Merge with neighbors */
    if (prev && chunk_get_status(prev) == CHUNK_FREE) {
        remove_from_bin(prev);
        chunk = merge_chunk(prev, chunk);
    }

    if (next && chunk_get_status(next) == CHUNK_FREE) {
        remove_from_bin(next);
        chunk = merge_chunk(chunk, next);
    }

    /* Insert merged chunk */
    if (chunk && is_valid_chunk_addr(chunk)) {
        insert_into_bin(chunk);
    }
}