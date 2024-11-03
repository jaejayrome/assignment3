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
#define MIN_SPLIT_UNITS (2 + FOOTER_UNITS)

static Chunk_T g_free_head = NULL;
static void *g_heap_start = NULL, *g_heap_end = NULL;

enum { MEMALLOC_MIN = 1024 };

/* Helper functions */
static size_t size_to_units(size_t size) {
    return (size + (CHUNK_UNIT - 1)) / CHUNK_UNIT;
}

static Chunk_T get_chunk_from_data_ptr(void *m) {
    return (Chunk_T)((char *)m - CHUNK_UNIT);
}

static void init_my_heap(void) {
    g_heap_start = g_heap_end = sbrk(0);
    if (g_heap_start == (void *)-1) {
        fprintf(stderr, "sbrk(0) failed\n");
        exit(-1);
    }
}

/* Fast merge function */
static Chunk_T merge_chunk(Chunk_T c1, Chunk_T c2) {
    // Basic validation
    if (!c1 || !c2 || (void*)c2 <= (void*)c1 || 
        (void*)c2 >= g_heap_end || chunk_get_status(c2) != CHUNK_FREE)
        return c1;
    
    size_t total_units = chunk_get_units(c1) + chunk_get_units(c2) + 1 + FOOTER_UNITS;
    chunk_set_units(c1, total_units - 1 - FOOTER_UNITS);
    chunk_set_next_free_chunk(c1, chunk_get_next_free_chunk(c2));
    chunk_set_footer(c1);
    return c1;
}

/* Simple split function */
static Chunk_T split_chunk(Chunk_T c, size_t units) {
    size_t total_units = chunk_get_units(c);
    size_t remaining = total_units - units - 1 - FOOTER_UNITS;
    
    if (remaining < MIN_SPLIT_UNITS) 
        return c;
        
    chunk_set_units(c, remaining);
    chunk_set_footer(c);
    
    Chunk_T c2 = (Chunk_T)((char *)c + (remaining + 1) * CHUNK_UNIT + FOOTER_SIZE);
    if ((void*)c2 >= g_heap_end) 
        return c;
        
    chunk_set_units(c2, units);
    chunk_set_status(c2, CHUNK_IN_USE);
    chunk_set_next_free_chunk(c2, NULL);
    chunk_set_footer(c2);
    
    return c2;
}

/* Fast insert function */
static void insert_chunk(Chunk_T c) {
    if (!c || (void*)c >= g_heap_end) return;

    chunk_set_status(c, CHUNK_FREE);
    chunk_set_footer(c);
    
    // Try to merge with next block if it's free
    Chunk_T next = chunk_get_next_adjacent(c, g_heap_start, g_heap_end);
    if (next && chunk_get_status(next) == CHUNK_FREE) {
        // If next is free list head, update head pointer
        if (next == g_free_head) {
            g_free_head = c;
            chunk_set_next_free_chunk(c, chunk_get_next_free_chunk(next));
            merge_chunk(c, next);
        } else {
            // Fast path: just link into list and merge
            chunk_set_next_free_chunk(c, g_free_head);
            g_free_head = c;
            merge_chunk(c, next);
        }
    } else {
        // Simple insert at head
        chunk_set_next_free_chunk(c, g_free_head);
        g_free_head = c;
    }
}

/* Fast remove */
static void remove_chunk_from_list(Chunk_T prev, Chunk_T c) {
    if (!c) return;

    if (prev)
        chunk_set_next_free_chunk(prev, chunk_get_next_free_chunk(c));
    else
        g_free_head = chunk_get_next_free_chunk(c);
    
    chunk_set_status(c, CHUNK_IN_USE);
    chunk_set_footer(c);
}

/* Efficient allocation */
static Chunk_T allocate_more_memory(size_t units) {
    size_t alloc_units = (units < MEMALLOC_MIN) ? MEMALLOC_MIN : units;
    size_t total_size = (alloc_units + 1) * CHUNK_UNIT + FOOTER_SIZE;
    
    Chunk_T c = (Chunk_T)sbrk(total_size);
    if (c == (Chunk_T)-1) return NULL;
    
    g_heap_end = sbrk(0);
    
    chunk_set_units(c, alloc_units);
    chunk_set_status(c, CHUNK_FREE);
    chunk_set_next_free_chunk(c, NULL);
    chunk_set_footer(c);
    
    insert_chunk(c);
    return c;
}

/* Malloc implementation */
void *heapmgr_malloc(size_t size) {
    if (size <= 0) return NULL;

    static int is_init = FALSE;
    if (!is_init) {
        init_my_heap();
        is_init = TRUE;
    }

    size_t units = size_to_units(size);
    Chunk_T prev = NULL;
    Chunk_T curr = g_free_head;

    // First fit search
    while (curr) {
        if (chunk_get_units(curr) >= units) {
            if (chunk_get_units(curr) > units + MIN_SPLIT_UNITS)
                curr = split_chunk(curr, units);
            remove_chunk_from_list(prev, curr);
            return (void *)((char *)curr + CHUNK_UNIT);
        }
        prev = curr;
        curr = chunk_get_next_free_chunk(curr);
    }

    // Need more memory
    curr = allocate_more_memory(units);
    if (!curr) return NULL;
    
    if (chunk_get_units(curr) > units + MIN_SPLIT_UNITS)
        curr = split_chunk(curr, units);
    remove_chunk_from_list(NULL, curr);
    
    return (void *)((char *)curr + CHUNK_UNIT);
}

/* Fast free implementation */
void heapmgr_free(void *m) {
    if (!m) return;
    
    Chunk_T c = get_chunk_from_data_ptr(m);
    if ((void*)c < g_heap_start || (void*)c >= g_heap_end ||
        chunk_get_status(c) != CHUNK_IN_USE) return;
        
    Footer_T footer = chunk_get_footer(c);
    if (!footer || footer->header != c) return;
    
    insert_chunk(c);
}