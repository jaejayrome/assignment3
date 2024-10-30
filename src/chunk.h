#ifndef _CHUNK_BASE_H_
#define _CHUNK_BASE_H_

#include <stdbool.h>
#include <unistd.h>

typedef struct Chunk *Chunk_T;
typedef struct ChunkFooter *Footer_T;

struct Chunk
{
    Chunk_T next; /* Next chunk in free list */
    Chunk_T prev;
    int units;  /* Capacity in chunk units */
    int status; /* CHUNK_FREE or CHUNK_IN_USE */
};

struct ChunkFooter
{
    Chunk_T header; /* Pointer back to chunk header */
};

enum
{
    CHUNK_FREE,
    CHUNK_IN_USE,
};

enum
{
    CHUNK_UNIT = 16,
};

/* Original functions */
int chunk_get_status(Chunk_T c);
void chunk_set_status(Chunk_T c, int status);
int chunk_get_units(Chunk_T c);
void chunk_set_units(Chunk_T c, int units);
Chunk_T chunk_get_next_free_chunk(Chunk_T c);
void chunk_set_next_free_chunk(Chunk_T c, Chunk_T next);

/* Modified functions to include bounds */
Chunk_T chunk_get_next_adjacent(Chunk_T c, void *start, void *end);
Footer_T chunk_get_footer(Chunk_T c);
void chunk_set_footer(Chunk_T c);
Chunk_T chunk_get_prev_from_footer(void *ptr, void *start);
int chunk_is_valid(Chunk_T c, void *start, void *end);

#endif
