#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "allocator.h"
#include "thread_pool.h"
#include "executor.h"

/* ── Test: Arena Allocator ───────────────────────────────── */
void test_arena() {
    printf("\n[TEST] Arena Allocator\n");

    ca_arena_t* arena = ca_arena_create(1024 * 1024); // 1MB
    assert(arena != NULL);

    /* Basic alloc */
    int* nums = (int*)ca_arena_alloc(arena, 10 * sizeof(int));
    assert(nums != NULL);
    for (int i = 0; i < 10; i++) nums[i] = i * 2;
    assert(nums[9] == 18);

    /* Zero alloc */
    char* buf = (char*)ca_arena_alloc_zero(arena, 64);
    assert(buf != NULL);
    for (int i = 0; i < 64; i++) assert(buf[i] == 0);

    /* String dup */
    char* str = ca_arena_strdup(arena, "CoreAgent rocks!");
    assert(str != NULL);
    assert(strcmp(str, "CoreAgent rocks!") == 0);

    /* Save / restore */
    ca_arena_mark_t mark = ca_arena_save(arena);
    void* tmp = ca_arena_alloc(arena, 512);
    assert(tmp != NULL);
    ca_arena_restore(arena, mark);
    assert(ca_arena_used(arena) == mark);

    ca_arena_print_stats(arena);
    ca_arena_destroy(arena);

    printf("[TEST] Arena Allocator PASSED ✅\n");
}

/* ── Test: Thread Pool ───────────────────────────────────── */
static volatile int task_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void increment_task(void* arg) {
    (void)arg;
    usleep(1000); // Simulate work (1ms)
    pthread_mutex_lock(&counter_mutex);
    task_counter++;
    pthread_mutex_unlock(&counter_mutex);
}

void test_thread_pool() {
    printf("\n[TEST] Thread Pool\n");

    ca_thread_pool_t* pool = ca_pool_create(4);
    assert(pool != NULL);

    task_counter = 0;
    int n_tasks  = 100;

    for (int i = 0; i < n_tasks; i++) {
        int ret = ca_pool_submit(pool, increment_task, NULL);
        assert(ret == 0);
    }

    ca_pool_wait(pool);

    assert(task_counter == n_tasks);
    assert(ca_pool_tasks_completed(pool) == (uint64_t)n_tasks);

    printf("[TEST] Thread Pool: %d tasks completed ✅\n", task_counter);
    ca_pool_destroy(pool);
}

/* ── Test: Process Executor ──────────────────────────────── */
void test_executor() {
    printf("\n[TEST] Process Executor\n");

    /* Test 1: simple echo */
    ca_exec_result_t r1 = ca_exec_shell("echo 'Hello CoreAgent'", 2000);
    assert(r1.status    == CA_EXEC_OK);
    assert(r1.exit_code == 0);
    assert(r1.stdout_data != NULL);
    assert(strstr(r1.stdout_data, "Hello CoreAgent") != NULL);
    printf("[TEST] echo output: %s", r1.stdout_data);
    ca_exec_result_free(&r1);

    /* Test 2: exit code */
    ca_exec_result_t r2 = ca_exec_shell("exit 42", 2000);
    assert(r2.status    == CA_EXEC_OK);
    assert(r2.exit_code == 42);
    ca_exec_result_free(&r2);

    /* Test 3: timeout enforcement */
    ca_exec_result_t r3 = ca_exec_shell("sleep 10", 500);
    assert(r3.status == CA_EXEC_TIMEOUT);
    printf("[TEST] Timeout enforced correctly ✅\n");
    ca_exec_result_free(&r3);

    printf("[TEST] Process Executor PASSED ✅\n");
}

/* ── Main ────────────────────────────────────────────────── */
int main(void) {
    printf("╔══════════════════════════════════════╗\n");
    printf("║   CoreAgent C Core Tests              ║\n");
    printf("╚══════════════════════════════════════╝\n");

    test_arena();
    test_thread_pool();
    test_executor();

    printf("\n✅ All C Core tests PASSED!\n\n");
    return 0;
}