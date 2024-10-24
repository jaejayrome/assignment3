#include "chunk.h"
#include <string.h>
#include <stdio.h>

int chunk_get_status(Header * c) {
    return c->status;
}

void chunk_set_status(Header * c, ChunkStatus status) {
    c->status = status;
    chunk_set_footer(c);
}

int chunk_get_units(Header * c) {
    return c->units;
}

void chunk_set_units(Header * c, int units) {
    c->units = units;
    chunk_set_footer(c);
}

Header * chunk_get_next_free_chunk(Header * c) {
    return c->next;
}

void chunk_set_next_free_chunk(Header * c, Header * next) {
    c->next = next;
}

Header * chunk_get_prev_free_chunk(Header * c) {
    return c->prev;
}

void chunk_set_prev_free_chunk(Header * c, Header * prev) {
    c->prev = prev;
}

void chunk_set_footer(Header * c) {
    char *footer_addr = (char *) c + (c->units + HEADER_UNITS) * CHUNK_UNIT;
    Footer *footer = (Footer *) footer_addr;
    footer->units = c->units;
}

int chunk_get_footer_units(Header * c) {
    char *footer_addr = (char *) c + (c->units + HEADER_UNITS) * CHUNK_UNIT;
    Footer *footer = (Footer *) footer_addr;
    return footer->units;
}

size_t chunk_total_size(Header * c) {
    return (HEADER_UNITS + c->units + FOOTER_UNITS) * CHUNK_UNIT;
}

Header * chunk_get_next_adjacent(Header * c, void *start, void *end) {
    char *next_addr = (char *) c + chunk_total_size(c);
    if ((void *) next_addr >= end) return NULL;
    return (Header *) next_addr;
}

Header * chunk_get_prev_adjacent(Header * c, void *start, void *end) {
    if ((void *) c <= start) return NULL;

    /* get the footer of the previous chunk */
    char *footer_addr = (char *) c - FOOTER_UNITS*CHUNK_UNIT;
    int prev_units = *((int *) (footer_addr));

    /* compute the start address of the previous chunk */
    size_t prev_chunk_size = (HEADER_UNITS + prev_units + FOOTER_UNITS) * CHUNK_UNIT;
    char *prev_addr = (char *) c - prev_chunk_size;

    if ((void *) prev_addr < start)
        return NULL;

    return (Header *) prev_addr;
}

#ifndef NDEBUG

int chunk_is_valid(Header * c, void *start, void *end) {
    if ((void *) c < start || (void *) c >= end) {
        fprintf(stderr, "Header at %p is out of heap bounds (%p - %p)\n", (void *) c, (void *) start, (void *) end);
        return 0;
    }

    if (c->units <= 0) {
        fprintf(stderr, "Header at %p has invalid units %d\n", (void *) c, c->units);
        return 0;
    }

    if (c->status != CHUNK_FREE && c->status != CHUNK_IN_USE) {
        fprintf(stderr, "Header at %p has invalid status %d\n", (void *) c, c->status);
        return 0;
    }

    /* check if the footer matches the header */
    int footer_units = chunk_get_footer_units(c);
    if (footer_units != c->units) {
        fprintf(stderr, "Header at %p has wrong footer (units: %d != %d)\n",
                (void *) c, c->units, footer_units);
        return 0;
    }

    return 1;
}

#endif
