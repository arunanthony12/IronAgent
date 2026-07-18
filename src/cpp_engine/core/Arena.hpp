#pragma once

#include "allocator.h"
#include <stdexcept>
#include <string>

namespace coreagent {

/* ─────────────────────────────────────────────────────────
 * Arena — RAII wrapper around ca_arena_t
 * Ensures arena is always destroyed, even on exceptions
 * ───────────────────────────────────────────────────────── */
class Arena {
public:
    explicit Arena(size_t capacity = CA_ARENA_DEFAULT_SIZE) {
        arena_ = ca_arena_create(capacity);
        if (!arena_)
            throw std::runtime_error("[Arena] Failed to allocate memory");
    }

    ~Arena() { ca_arena_destroy(arena_); }

    /* Non-copyable */
    Arena(const Arena&)            = delete;
    Arena& operator=(const Arena&) = delete;

    /* Movable */
    Arena(Arena&& other) noexcept : arena_(other.arena_) {
        other.arena_ = nullptr;
    }

    /* Allocation */
    void* alloc    (size_t size)        { return ca_arena_alloc(arena_, size);      }
    void* allocZero(size_t size)        { return ca_arena_alloc_zero(arena_, size); }
    char* strdup   (const char* str)    { return ca_arena_strdup(arena_, str);      }
    void  reset    ()                   { ca_arena_reset(arena_);                   }

    /* Info */
    size_t used()      const { return ca_arena_used(arena_);      }
    size_t remaining() const { return ca_arena_remaining(arena_); }
    void   printStats()const { ca_arena_print_stats(arena_);      }

    /* Save / restore position for temp allocations */
    ca_arena_mark_t save()                         { return ca_arena_save(arena_);         }
    void            restore(ca_arena_mark_t mark)  { ca_arena_restore(arena_, mark);       }

    ca_arena_t* raw() { return arena_; }

private:
    ca_arena_t* arena_ = nullptr;
};

} // namespace coreagent