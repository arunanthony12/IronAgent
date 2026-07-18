#ifndef CA_EXECUTOR_H
#define CA_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus          /* ← ADD THIS BLOCK */
extern "C" {
#endif

#define CA_EXEC_MAX_OUTPUT      (1024 * 1024)
#define CA_EXEC_MAX_ARGS        64
#define CA_EXEC_DEFAULT_TIMEOUT_MS  5000

typedef enum {
    CA_EXEC_OK       = 0,
    CA_EXEC_ERROR    = -1,
    CA_EXEC_TIMEOUT  = -2,
    CA_EXEC_OOM      = -3,
} ca_exec_status_t;

typedef struct {
    const char* command;
    const char* args[CA_EXEC_MAX_ARGS];
    const char* env_vars[64];
    const char* working_dir;
    const char* stdin_data;
    uint32_t    timeout_ms;
    int         capture_stderr;
} ca_exec_config_t;

typedef struct {
    char*            stdout_data;
    char*            stderr_data;
    size_t           stdout_len;
    size_t           stderr_len;
    int              exit_code;
    uint32_t         elapsed_ms;
    ca_exec_status_t status;
} ca_exec_result_t;

ca_exec_result_t ca_exec_run(const ca_exec_config_t* config);
ca_exec_result_t ca_exec_shell(const char* cmd, uint32_t timeout_ms);
void             ca_exec_result_free(ca_exec_result_t* result);
ca_exec_config_t ca_exec_default_config(void);

#ifdef __cplusplus          /* ← ADD THIS BLOCK */
}
#endif

#endif /* CA_EXECUTOR_H */