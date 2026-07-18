#include "allocator.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ── Internal: align size up to CA_ALIGN boundary ───────── */
static inline size_t align_up(size_t size) {
    return (size + (CA_ALIGN - 1)) & ~(size_t)(CA_ALIGN - 1);
}

/* ── Create arena ────────────────────────────────────────── */
ca_arena_t* ca_arena_create(size_t capacity) {
    if (capacity == 0) capacity = CA_ARENA_DEFAULT_SIZE;

    ca_arena_t* arena = NULL;
    /* Align the struct itself to 64-byte cache line */
    if (posix_memalign((void**)&arena, 64, sizeof(ca_arena_t)) != 0) return NULL;

    /* Align the memory pool to 32 bytes for AVX2 */
    if (posix_memalign((void**)&arena->base, CA_ALIGN, capacity) != 0) {
        free(arena);
        return NULL;
    }

    arena->capacity    = capacity;
    arena->offset      = 0;
    arena->peak        = 0;
    arena->alloc_count = 0;

    return arena;
}

/* ── Destroy arena ───────────────────────────────────────── */
void ca_arena_destroy(ca_arena_t* arena) {
    if (!arena) return;
    free(arena->base); /* posix_memalign memory is safely freed with standard free() */
    free(arena);
}

/* ── Reset arena (keep memory, reset offset) ─────────────── */
void ca_arena_reset(ca_arena_t* arena) {
    assert(arena);
    arena->offset      = 0;
    arena->alloc_count = 0;
    /* peak is kept — useful for profiling across resets */
}

/* ── Core allocation ─────────────────────────────────────── */
void* ca_arena_alloc(ca_arena_t* arena, size_t size) {
    assert(arena);
    if (size == 0) return NULL;

    size_t aligned_size = align_up(size);

    if (arena->offset + aligned_size > arena->capacity) {
        fprintf(stderr,
            "[CoreAgent] Arena OOM: requested=%zu, used=%zu, cap=%zu\n",
            aligned_size, arena->offset, arena->capacity);
        return NULL;
    }

    void* ptr         = arena->base + arena->offset;
    arena->offset    += aligned_size;
    arena->alloc_count++;

    if (arena->offset > arena->peak)
        arena->peak = arena->offset;

    return ptr;
}

/* ── Zero-initialized allocation ─────────────────────────── */
void* ca_arena_alloc_zero(ca_arena_t* arena, size_t size) {
    void* ptr = ca_arena_alloc(arena, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

/* ── String duplicate into arena ─────────────────────────── */
char* ca_arena_strdup(ca_arena_t* arena, const char* str) {
    if (!str) return NULL;
    size_t len  = strlen(str) + 1;
    char*  copy = (char*)ca_arena_alloc(arena, len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

/* ── Usage info ──────────────────────────────────────────── */
size_t ca_arena_used(const ca_arena_t* arena) {
    return arena ? arena->offset : 0;
}

size_t ca_arena_remaining(const ca_arena_t* arena) {
    return arena ? arena->capacity - arena->offset : 0;
}

void ca_arena_print_stats(const ca_arena_t* arena) {
    if (!arena) return;
    printf("[Arena Stats]\n");
    printf("  Capacity   : %zu bytes (%.2f MB)\n",
           arena->capacity, arena->capacity / (1024.0 * 1024.0));
    printf("  Used       : %zu bytes (%.1f%%)\n",
           arena->offset,
           100.0 * arena->offset / arena->capacity);
    printf("  Peak       : %zu bytes\n", arena->peak);
    printf("  Alloc count: %u\n", arena->alloc_count);
}

/* ── Save / Restore position ─────────────────────────────── */
ca_arena_mark_t ca_arena_save(const ca_arena_t* arena) {
    return arena->offset;
}

void ca_arena_restore(ca_arena_t* arena, ca_arena_mark_t mark) {
    assert(arena);
    assert(mark <= arena->offset);
    arena->offset = mark;
}