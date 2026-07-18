#pragma once

#include "thread_pool.h"
#include <stdexcept>
#include <cstdint>

namespace coreagent {

/* ─────────────────────────────────────────────────────────
 * ThreadPool — RAII wrapper around ca_thread_pool_t
 * Used to run agent tools in parallel
 * ───────────────────────────────────────────────────────── */
class ThreadPool {
public:
    explicit ThreadPool(uint32_t num_threads = 4) {
        pool_ = ca_pool_create(num_threads);
        if (!pool_)
            throw std::runtime_error("[ThreadPool] Failed to create pool");
    }

    ~ThreadPool() { ca_pool_destroy(pool_); }

    /* Non-copyable */
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /* Submit a task to the pool */
    int  submit(ca_task_fn fn, void* arg) { return ca_pool_submit(pool_, fn, arg); }

    /* Block until all tasks finish */
    void wait() { ca_pool_wait(pool_); }

    /* Stats */
    uint64_t tasksCompleted() const { return ca_pool_tasks_completed(pool_); }
    uint32_t activeThreads()  const { return ca_pool_active_threads(pool_);  }

private:
    ca_thread_pool_t* pool_ = nullptr;
};

} // namespace coreagent