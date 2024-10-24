#ifndef CHUNKBASE_H
#define CHUNKBASE_H

#include <stddef.h> // for size_t
#include <assert.h> // for assert

/* Define the Chunk_T structure representing a memory chunk */
typedef struct Chunk *Chunk_T;

/* Structure representing a chunk in memory */
struct Chunk
{
    Chunk_T next; /* Pointer to the next free chunk */
    int units;    /* Number of chunk units */
    int status;   /* CHUNK_FREE or CHUNK_IN_USE */
};

enum
{
    CHUNK_UNIT = 16, /* 16 = sizeof(struct Chunk) */
};

/* Status constants */
#define CHUNK_FREE 0
#define CHUNK_IN_USE 1

Chunk_T get_chunk_footer(Chunk_T c);

/* Get and set chunk status */
int chunk_get_status(Chunk_T c);
void chunk_set_status(Chunk_T c, int status);

/* Get and set chunk units */
int chunk_get_units(Chunk_T c);
void chunk_set_units(Chunk_T c, int units);

/* Get and set next free chunk */
Chunk_T chunk_get_next_free_chunk(Chunk_T c);
void chunk_set_next_free_chunk(Chunk_T c, Chunk_T next);

/* Get and set previous free chunk (treated as footer) */
Chunk_T chunk_get_prev_free_chunk(Chunk_T c);
void chunk_set_prev_free_chunk(Chunk_T c, Chunk_T prev);

/* Get the next adjacent chunk in memory */
Chunk_T chunk_get_next_adjacent(Chunk_T c, void *start, void *end);

/* Get the previous adjacent chunk in memory (footer logic) */
Chunk_T chunk_get_prev_adjacent(Chunk_T c, void *start, void *end);

/* Get the next free chunk in the free list */
Chunk_T chunk_get_next_free(Chunk_T c);

/* Get the previous free chunk in the free list (using footer) */
Chunk_T chunk_get_prev_free(Chunk_T c);

/* Debug: Check if a chunk is valid */
#ifndef NDEBUG
int chunk_is_valid(Chunk_T c, void *start, void *end);
#endif

#endif /* CHUNKBASE_H */
