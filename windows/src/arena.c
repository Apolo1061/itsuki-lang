#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ARENA_DEFAULT_CHUNK_SIZE (2 * 1024 * 1024) // 2 MB por chunk
#define ARENA_ALIGNMENT 8
#define ARENA_ALIGN(size) (((size) + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1))

Arena* g_compiler_arena = NULL;

Arena* arena_create(size_t chunk_size) {
    if (chunk_size == 0) chunk_size = ARENA_DEFAULT_CHUNK_SIZE;
    Arena* arena = malloc(sizeof(Arena));
    if (!arena) return NULL;
    arena->head = NULL;
    arena->default_chunk_size = chunk_size;
    arena->total_allocated = 0;
    return arena;
}

static ArenaChunk* arena_new_chunk(size_t capacity) {
    ArenaChunk* chunk = malloc(sizeof(ArenaChunk) + capacity);
    if (!chunk) return NULL;
    chunk->capacity = capacity;
    chunk->used = 0;
    chunk->next = NULL;
    return chunk;
}

void* arena_alloc(Arena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    size = ARENA_ALIGN(size);

    if (!arena->head || (arena->head->capacity - arena->head->used) < size) {
        size_t next_capacity = arena->default_chunk_size;
        if (size > next_capacity) next_capacity = size;
        ArenaChunk* new_chunk = arena_new_chunk(next_capacity);
        if (!new_chunk) return NULL;
        new_chunk->next = arena->head;
        arena->head = new_chunk;
    }

    void* ptr = (void*)(arena->head->data + arena->head->used);
    arena->head->used += size;
    arena->total_allocated += size;
    return ptr;
}

void* arena_calloc(Arena* arena, size_t count, size_t size) {
    size_t total = count * size;
    void* ptr = arena_alloc(arena, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

char* arena_strdup(Arena* arena, const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

char* arena_strndup(Arena* arena, const char* s, size_t len) {
    if (!s) return NULL;
    char* copy = arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

void arena_reset(Arena* arena) {
    if (!arena) return;
    ArenaChunk* curr = arena->head;
    while (curr) {
        curr->used = 0;
        curr = curr->next;
    }
    arena->total_allocated = 0;
}

void arena_destroy(Arena* arena) {
    if (!arena) return;
    ArenaChunk* curr = arena->head;
    while (curr) {
        ArenaChunk* next = curr->next;
        free(curr);
        curr = next;
    }
    free(arena);
}

void compiler_arena_init(void) {
    if (!g_compiler_arena) {
        g_compiler_arena = arena_create(ARENA_DEFAULT_CHUNK_SIZE);
    }
}

void compiler_arena_free(void) {
    if (g_compiler_arena) {
        arena_destroy(g_compiler_arena);
        g_compiler_arena = NULL;
    }
}

void* c_alloc(size_t size) {
    if (!g_compiler_arena) compiler_arena_init();
    return arena_alloc(g_compiler_arena, size);
}

void* c_calloc(size_t count, size_t size) {
    if (!g_compiler_arena) compiler_arena_init();
    return arena_calloc(g_compiler_arena, count, size);
}

char* c_strdup(const char* s) {
    if (!g_compiler_arena) compiler_arena_init();
    return arena_strdup(g_compiler_arena, s);
}

char* c_strndup(const char* s, size_t len) {
    if (!g_compiler_arena) compiler_arena_init();
    return arena_strndup(g_compiler_arena, s, len);
}
