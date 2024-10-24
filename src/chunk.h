#ifndef _CHUNK_H_
#define _CHUNK_H_

#pragma once

#include <stdbool.h>
#include <unistd.h>

typedef enum {
    CHUNK_FREE,
    CHUNK_IN_USE,
} ChunkStatus;

struct Header {
    int units;            // capacity of a chunk in units
    ChunkStatus status;   // CHUNK_FREE or CHUNK_IN_USE
    struct Header * next; // next chunk in the free list
    struct Header * prev; // previous chunk in the free list
};
typedef struct Header Header;

struct Footer {
    int units;
};
typedef struct Footer Footer;

#define INT_CEIL(x, y) (((x)+(y)-1)/(y))

// Size of allocation chunks
#define CHUNK_UNIT 16

// Size of the header and footer in units
#define HEADER_UNITS INT_CEIL(sizeof(Header), CHUNK_UNIT)
#define FOOTER_UNITS INT_CEIL(sizeof(Footer), CHUNK_UNIT)

// Amount of control units for each chunk
#define EXTRA_UNITS HEADER_UNITS + FOOTER_UNITS

// Functions
int chunk_get_status(Header * c);
void chunk_set_status(Header * c, ChunkStatus status);
int chunk_get_units(Header * c);
void chunk_set_units(Header * c, int units);
Header * chunk_get_next_free_chunk(Header * c);
void chunk_set_next_free_chunk(Header * c, Header * next);
Header * chunk_get_prev_free_chunk(Header * c);
void chunk_set_prev_free_chunk(Header * c, Header * prev);
Header * chunk_get_next_adjacent(Header * c, void *start, void *end);
Header * chunk_get_prev_adjacent(Header * c, void *start, void *end);
void chunk_set_footer(Header * c);
int chunk_get_footer_units(Header * c);
int chunk_get_footer_status(Header * c);
size_t chunk_total_size(Header * c);

#ifndef NDEBUG

int chunk_is_valid(Header * c, void *start, void *end);

#endif /* NDEBUG */

#endif /* _CHUNK_H_ */
