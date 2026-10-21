#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct ArenaChunk {
    size_t capacity;
    size_t used;
    struct ArenaChunk* next;
    char data[];
} ArenaChunk;

typedef struct Arena {
    ArenaChunk* head;
    size_t default_chunk_size;
    size_t total_allocated;
} Arena;

// Creación y ciclo de vida de una arena
Arena* arena_create(size_t chunk_size);
void* arena_alloc(Arena* arena, size_t size);
void* arena_calloc(Arena* arena, size_t count, size_t size);
char* arena_strdup(Arena* arena, const char* s);
char* arena_strndup(Arena* arena, const char* s, size_t len);
void arena_reset(Arena* arena);
void arena_destroy(Arena* arena);

// Arena global del compilador para AST, Tokens y Strings
extern Arena* g_compiler_arena;

void compiler_arena_init(void);
void compiler_arena_free(void);

// Helpers directos que usan la arena global del compilador
void* c_alloc(size_t size);
void* c_calloc(size_t count, size_t size);
char* c_strdup(const char* s);
char* c_strndup(const char* s, size_t len);

#endif // ARENA_H
