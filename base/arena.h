#pragma once

#include <base_types.h>

// An opaque data type for the arena allocator.
typedef struct arena_s Arena;

// Forward-declare the internal chunk struct. This is needed for the arena_pos_t
// but keeps the full definition private to the .c file.
struct arena_chunk;

/**
 * @brief A handle representing a specific position within the arena.
 * Use this to save the arena's state and later reset back to it.
 */
typedef struct {
    struct arena_chunk *chunk;
    char *ptr;
} arena_pos_t;


/**
 * @brief Creates a new arena allocator.
 *
 * This function initializes an arena and allocates an initial memory chunk
 * from the buddy allocator.
 * @param initial_size The suggested size for the first chunk. The actual size
 * may be larger to meet alignment and minimum size requirements.
 * @return A pointer to the newly created arena, or NULL on failure.
 */
Arena *arena_new(size_t initial_size);

/**
 * @brief Allocates a block of memory from the arena.
 *
 * Memory is allocated using a bump pointer for high efficiency. If the current
 * chunk is full, the arena will advance to the next chunk (if available after a
 * reset) or allocate a new one from the buddy system.
 *
 * @param arena A pointer to the arena.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, aligned to 16 bytes.
 *
 * The returned pointer is always valid. If the allocation fails it aborts.
 */
void *arena_alloc(Arena *arena, size_t size);

/**
 * @brief Captures the current allocation position in the arena.
 *
 * @param arena A pointer to the arena.
 * @return An arena_pos_t handle that can be used with arena_reset_to().
 */
arena_pos_t arena_get_pos(Arena *arena);

/**
 * @brief Resets the arena's allocation pointer to a previously saved position.
 *
 * This invalidates all allocations made since the position was saved,
 * making that memory available for new allocations.
 *
 * @param arena A pointer to the arena.
 * @param pos The saved position to restore.
 */
void arena_reset(Arena *arena, arena_pos_t pos);

/**
 * @brief Deallocates all memory used by the arena.
 *
 * This function iterates through all chunks owned by the arena, frees them
 * using `buddy_free`, and finally frees the arena structure itself.
 * The arena pointer is invalid after this call.
 *
 * @param arena A pointer to the arena.
 */
void arena_free(Arena *arena);

/**
 * @brief Returns the total number of chunks in the arena.
 *
 * This function walks the chunk linked list to count all chunks.
 * Useful for testing and debugging to verify arena expansion.
 *
 * @param arena A pointer to the arena.
 * @return The total number of chunks, or 0 if arena is NULL.
 */
size_t arena_chunk_count(Arena *arena);

/**
 * @brief Returns the index of the current chunk (0-based).
 *
 * This function walks from the first chunk to the current chunk,
 * counting the number of steps. The first chunk has index 0,
 * the second chunk has index 1, and so on.
 * Useful for testing to verify which chunk allocations are in.
 *
 * @param arena A pointer to the arena.
 * @return The index of the current chunk (0-based), or 0 if arena is NULL.
 */
size_t arena_current_chunk_index(Arena *arena);

/**
 * @brief Convenience macro to allocate an array of elements from the arena.
 *
 * This macro allocates memory for 'count' elements of 'type' and returns
 * a pointer to the first element, cast to the appropriate type.
 *
 * @param arena A pointer to the arena.
 * @param type The type of elements to allocate.
 * @param count The number of elements to allocate.
 * @return A pointer to the allocated array, cast to 'type*'.
 */
#define arena_alloc_array(arena, type, count) ((type*)arena_alloc((arena), sizeof(type) * (count)))
