/*--------------------------------------------------------------------*/
/* heapmgr1.c                                                         */
/* Author: Jerome Goh                                            */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "chunk.h"

#define FALSE 0
#define TRUE  1

enum {
    MEMALLOC_MIN = 1024,
};

// g_free_head: point to first chunk in the free list
static Header * g_free_head = NULL;

// g_heap_start, g_heap_end: start and end of the heap area.
// g_heap_end will move if you increase the heap
static void *g_heap_start = NULL;
static void *g_heap_end   = NULL;

static void insert_chunk(Header * c);
static void remove_chunk_from_list(Header * c);

#ifndef NDEBUG

// Validity check for entire data structures for chunks.
static int check_heap_validity(void) {
    Header * w;

    if (g_heap_start == NULL) {
        fprintf(stderr, "Uninitialized heap start\n");
        return FALSE;
    }

    if (g_heap_end == NULL) {
        fprintf(stderr, "Uninitialized heap end\n");
        return FALSE;
    }

    if (g_heap_start == g_heap_end) {
        if (g_free_head == NULL)
            return 1;
        fprintf(stderr, "Inconsistent empty heap\n");
        return FALSE;
    }

    // Check all chunks in the heap
    for (w = (Header *) g_heap_start;
         w && (void *) w < g_heap_end;
         w = chunk_get_next_adjacent(w, g_heap_start, g_heap_end)) {

        if (!chunk_is_valid(w, g_heap_start, g_heap_end))
            return 0;
    }

    // Check the free list
    for (w = g_free_head; w; w = chunk_get_next_free_chunk(w)) {
        if (chunk_get_status(w) != CHUNK_FREE) {
            fprintf(stderr, "Non-free chunk in the free chunk list\n");
            return 0;
        }

        if (!chunk_is_valid(w, g_heap_start, g_heap_end))
            return 0;

        // Make sure the doubly linked list is valid
        if (chunk_get_next_free_chunk(w)) {
            assert( chunk_get_prev_free_chunk(chunk_get_next_free_chunk(w)) == w
                    && "Linked list is broken");
        }
    }
    return TRUE;
}

#endif

// Returns the number of units for 'size' bytes
static size_t size_to_units(size_t size) {
    return INT_CEIL(size, CHUNK_UNIT);
}

// Returns the header pointer that contains data m
static Header * get_chunk_from_data_ptr(void *m) {
    return (Header *) ((char *) m - sizeof(struct Header));
}

// Initialize data structures and global variables for
// chunk management
static void init_my_heap(void) {
    /* initialize g_heap_start and g_heap_end */
    g_heap_start = g_heap_end = sbrk(0);
    if (g_heap_start == (void *) -1) {
        fprintf(stderr, "sbrk(0) failed\n");
        exit(-1);
    }
}

// Merge two adjacent chunks and return the merged chunk.
static Header * merge_chunk(Header * c1, Header * c2) {
    /* c1 and c2 must be adjacent */
    assert(chunk_get_next_adjacent(c1, g_heap_start, g_heap_end) == c2);
    assert(chunk_get_status(c1) == CHUNK_FREE);
    assert(chunk_get_status(c2) == CHUNK_FREE);

    /* remove c2 from free list */
    remove_chunk_from_list(c2);

    /* adjust the units of c1 */
    size_t c2_total_units = c2->units + EXTRA_UNITS;

    chunk_set_units(c1, c1->units + c2_total_units);
    chunk_set_footer(c1);

    return c1;
}

// Split 'c' into two chunks. The first chunk remains in the free list,
// and the second chunk is returned for allocation.
static Header * split_chunk(Header * c, size_t units) {
    Header * c2;
    size_t original_units = chunk_get_units(c);
    size_t overhead_units = EXTRA_UNITS;

    assert(chunk_get_status(c) == CHUNK_FREE);
    assert(original_units >= units);

    /* remove c from the free list before changing its status */
    remove_chunk_from_list(c);

    /* calculate units for the remaining free chunk */
    size_t c2_units = original_units - units;

    /* determine if splitting is possible */
    if (c2_units >= overhead_units + 1) {
        /* adjust the size and status of the allocated chunk */
        chunk_set_units(c, units);
        chunk_set_status(c, CHUNK_IN_USE);
        chunk_set_footer(c);

        /* prepare the remaining free chunk */
        c2 = (Header *)((char *)c + ((units + EXTRA_UNITS) * CHUNK_UNIT));
        chunk_set_units(c2, c2_units - overhead_units);
        chunk_set_status(c2, CHUNK_FREE);
        chunk_set_footer(c2);

        /* insert the new free chunk into the free list */
        insert_chunk(c2);
    } else {
        /* not enough space to split; allocate the whole chunk */
        /* c has already been removed from the free list */
        chunk_set_units(c, original_units);
        chunk_set_status(c, CHUNK_IN_USE);
        chunk_set_footer(c);
        c2 = NULL;
    }

    return c;
}

// Insert a chunk 'c' into the free list.
static void insert_chunk(Header * c) {
    assert (chunk_get_units(c) >= 1);

    chunk_set_status(c, CHUNK_FREE);

    Header * prev = NULL;
    Header * curr = g_free_head;

    while (curr != NULL && curr < c) {
        prev = curr;
        curr = curr->next;
    }

    // insert 'c' between 'prev' and 'curr'
    c->prev = prev;
    c->next = curr;

    if (prev != NULL)
        prev->next = c;
    else
        g_free_head = c;

    if (curr != NULL)
        curr->prev = c;

    // try to coalesce with previous chunk
    if (prev != NULL && (char *)prev + chunk_total_size(prev) == (char *)c) {
        c = merge_chunk(prev, c);
    }

    // try to coalesce with next chunk
    if (curr != NULL && (char *)c + chunk_total_size(c) == (char *)curr) {
        c = merge_chunk(c, curr);
    }

    // after coalescing, update the free list pointers
    if (c->prev != NULL)
        c->prev->next = c;
    else
        g_free_head = c;

    if (c->next != NULL)
        c->next->prev = c;
}

// Removes 'c' from the free chunk list
static void remove_chunk_from_list(Header * c) {
    assert (chunk_get_status(c) == CHUNK_FREE);

    if (c->prev)
        c->prev->next = c->next;
    else
        g_free_head = c->next;

    if (c->next)
        c->next->prev = c->prev;

    c->next = c->prev = NULL;
}

// Allocate a new chunk by increasing the heap
static Header * allocate_more_memory(size_t units) {
    Header * c;
    size_t overhead_units = EXTRA_UNITS;
    size_t total_units = units + overhead_units;

    if (total_units < MEMALLOC_MIN)
        total_units = MEMALLOC_MIN;

    size_t size = total_units * CHUNK_UNIT;

    c = (Header *)sbrk(size);
    if (c == (Header *)-1)
        return NULL;

    g_heap_end = sbrk(0);

    /* set the units to total_units - overhead_units */
    chunk_set_units(c, total_units - overhead_units);
    chunk_set_status(c, CHUNK_FREE);
    c->next = c->prev = NULL;
    chunk_set_footer(c);

    /* insert the new chunk into the free list */
    insert_chunk(c);

    return c;
}

// Dynamically allocate memory capable of holding size bytes.
void * heapmgr_malloc(size_t size) {
    static int is_init = FALSE;
    Header * c;
    size_t units;
    size_t overhead_units = EXTRA_UNITS;

    if (size <= 0)
        return NULL;

    if (is_init == FALSE) {
        init_my_heap();
        is_init = TRUE;
    }

    /* see if everything is OK before doing any operations */
    assert(check_heap_validity());

    units = size_to_units(size);

    // search for a suitable chunk in the free list
    for (c = g_free_head; c != NULL; c = c->next) {
        if (chunk_get_units(c) >= units) {
            size_t c_units = chunk_get_units(c);
            if (c_units >= units + overhead_units + 1) {
                /* split the chunk */
                c = split_chunk(c, units);
                assert(check_heap_validity());
                return (void *)((char *)c + sizeof(struct Header));
            } else {
                /* allocate the entire chunk */
                remove_chunk_from_list(c);
                chunk_set_status(c, CHUNK_IN_USE);
                chunk_set_footer(c);
                assert(check_heap_validity());
                return (void *)((char *)c + sizeof(struct Header));
            }
        }
    }

    /* allocate new memory */
    c = allocate_more_memory(units);
    if (c == NULL) {
        assert(check_heap_validity());
        return NULL;
    }

    /* after allocating more memory, we can try to allocate again */
    return heapmgr_malloc(size);
}


// Releases dynamically allocated memory.
void heapmgr_free(void *m) {
    if (m == NULL) return;

    // ensure heap validity before operation
    assert(check_heap_validity());

    Header * c = get_chunk_from_data_ptr(m);
    assert(chunk_get_status(c) == CHUNK_IN_USE);

    chunk_set_status(c, CHUNK_FREE);

    // insert 'c' into the free list in order
    insert_chunk(c);

    assert(check_heap_validity());
}
