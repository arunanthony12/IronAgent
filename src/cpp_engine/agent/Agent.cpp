#include "Agent.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <sstream>

namespace coreagent {

/* ── Constructor ─────────────────────────────────────────── */
Agent::Agent(const AgentConfig& config)
    : config_(config),
      state_(AgentState::IDLE),
      context_(config.context_arena_capacity, config.context_scratch_capacity, config.max_tokens),
      thread_pool_(config.num_threads)
{
    context_.addSystem(config.system_prompt);
    setupBuiltinTools();

    printf("\n[%s] ✅ Agent initialized\n",   config_.name.c_str());
    printf("[%s]    Threads  : %u\n",         config_.name.c_str(), config_.num_threads);
    printf("[%s]    MaxSteps : %u\n",         config_.name.c_str(), config_.max_steps);
    printf("[%s]    MaxTokens: %zu\n\n",      config_.name.c_str(), config_.max_tokens);
}

/* ── State transition ────────────────────────────────────── */
void Agent::setState(AgentState s) {
    printf("[%s] %-12s → %s\n",
           config_.name.c_str(),
           agentStateToString(state_).c_str(),
           agentStateToString(s).c_str());
    state_ = s;
}

/* ── Register a tool ─────────────────────────────────────── */
void Agent::registerTool(const std::string& name,
                          const std::string& description,
                          ToolFn fn) {
    registry_.registerTool(name, description, fn);
}

/* ── Built-in tools ──────────────────────────────────────── */
void Agent::setupBuiltinTools() {

    /* Tool 1: shell — run bash command */
    registry_.registerTool("shell",
        "Execute a shell command, returns stdout",
        [this](const ToolInput& input) -> ToolOutput {
            auto r = Executor::shell(input.args, config_.tool_timeout);
            return ToolOutput{
                r.stdout_data,
                r.ok(),
                r.stderr_data,
                r.elapsed_ms
            };
        }
    );

    /* Tool 2: read_file — read a file from disk */
    registry_.registerTool("read_file",
        "Read contents of a file. args = file path",
        [](const ToolInput& input) -> ToolOutput {
            std::ifstream f(input.args);
            if (!f.is_open())
                return ToolOutput{"", false,
                    "Cannot open file: " + input.args, 0};
            std::ostringstream ss;
            ss << f.rdbuf();
            return ToolOutput{ss.str(), true, "", 0};
        }
    );

    /* Tool 3: write_file — write content to a file */
    registry_.registerTool("write_file",
        "Write content to a file. args = 'path|content'",
        [](const ToolInput& input) -> ToolOutput {
            /* Parse: everything before first | is path */
            size_t sep = input.args.find('|');
            if (sep == std::string::npos)
                return ToolOutput{"", false,
                    "Format: 'path|content'", 0};

            std::string path    = input.args.substr(0, sep);
            std::string content = input.args.substr(sep + 1);

            std::ofstream f(path);
            if (!f.is_open())
                return ToolOutput{"", false,
                    "Cannot write file: " + path, 0};
            f << content;
            return ToolOutput{"Written: " + path, true, "", 0};
        }
    );

    /* Tool 4: echo — simple passthrough for testing */
    registry_.registerTool("echo",
        "Echo back the input args. Used for testing.",
        [](const ToolInput& input) -> ToolOutput {
            return ToolOutput{input.args, true, "", 0};
        }
    );
}

/* ── THINK: analyze the task ─────────────────────────────── */
std::string Agent::think(const std::string& task) {
    setState(AgentState::THINKING);

    context_.addUser(task);

    /* In a real agent this calls an LLM.
     * For now: produce a structured thought about the task */
    std::string thought =
        "Task received: [" + task + "]. "
        "I will analyze what tools are needed and form a plan.";

    context_.addAssistant(thought);

    printf("[%s] 💭 Thought: %s\n\n",
           config_.name.c_str(), thought.c_str());

    return thought;
}

/* ── PLAN: decide which tool + args to use ───────────────── */
std::string Agent::plan(const std::string& thought) {
    setState(AgentState::PLANNING);

    /* Simple rule-based planner (LLM replaces this in Phase 3)
     * Looks for keywords in the thought/task to pick a tool */

    std::string prompt(context_.buildPrompt());
    std::string plan_str;

    /* Keyword-based tool selection from context */
    if (prompt.find("list") != std::string::npos ||
        prompt.find("ls")   != std::string::npos ||
        prompt.find("files") != std::string::npos) {
        plan_str = "shell|ls -la";
    }
    else if (prompt.find("date") != std::string::npos ||
             prompt.find("time") != std::string::npos) {
        plan_str = "shell|date";
    }
    else if (prompt.find("disk") != std::string::npos ||
             prompt.find("space") != std::string::npos) {
        plan_str = "shell|df -h";
    }
    else if (prompt.find("memory") != std::string::npos ||
             prompt.find("ram")  != std::string::npos) {
        plan_str = "shell|free -h";
    }
    else if (prompt.find("read") != std::string::npos) {
        /* Extract filename if mentioned */
        plan_str = "echo|No file specified";
    }
    else {
        /* Default: echo back the thought */
        plan_str = "echo|" + thought;
    }

    printf("[%s] 📋 Plan: %s\n\n",
           config_.name.c_str(), plan_str.c_str());

    return plan_str;
}

/* ── ACT: execute the chosen tool ────────────────────────── */
ToolOutput Agent::act(const std::string& plan_str) {
    setState(AgentState::ACTING);

    /* Parse plan_str: "tool_name|args" */
    size_t sep = plan_str.find('|');
    std::string tool_name = (sep != std::string::npos)
                            ? plan_str.substr(0, sep)
                            : plan_str;
    std::string tool_args = (sep != std::string::npos)
                            ? plan_str.substr(sep + 1)
                            : "";

    ToolInput input{tool_name, tool_args};

    printf("[%s] 🔧 Calling tool: [%s] with args: [%s]\n",
           config_.name.c_str(),
           tool_name.c_str(),
           tool_args.c_str());

    ToolOutput output = registry_.dispatch(input);

    /* Store result in context */
    context_.addToolResult(tool_name,
        output.success ? output.result : ("ERROR: " + output.error));

    return output;
}

/* ── REFLECT: evaluate result, decide if done ────────────── */
bool Agent::reflect(const ToolOutput& result, std::string& final_output) {
    setState(AgentState::REFLECTING);

    if (result.success) {
        final_output = result.result;
        printf("[%s] ✅ Tool succeeded in %ums\n",
               config_.name.c_str(), result.elapsed_ms);
        printf("[%s] 📄 Result:\n%s\n",
               config_.name.c_str(), result.result.c_str());
        return true;   /* Done! */
    } else {
        printf("[%s] ❌ Tool failed: %s\n",
               config_.name.c_str(), result.error.c_str());
        return false;  /* Try again */
    }
}

/* ── RUN: full agent loop ────────────────────────────────── */
std::string Agent::run(const std::string& task) {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║  [%s] Starting task                  \n", config_.name.c_str());
    printf("║  Task: %-35s\n", task.c_str());
    printf("╚══════════════════════════════════════════╝\n\n");

    setState(AgentState::IDLE);

    std::string final_output;
    uint32_t    steps = 0;

    while (steps < config_.max_steps) {
        steps++;
        printf("[%s] ── Step %u/%u ───────────────────────\n\n",
               config_.name.c_str(), steps, config_.max_steps);

        /* THINK → PLAN → ACT → REFLECT */
        std::string thought  = think(task);
        std::string plan_str = plan(thought);
        ToolOutput  result   = act(plan_str);
        bool        done     = reflect(result, final_output);

        if (done) {
            setState(AgentState::DONE);
            break;
        }

        if (steps >= config_.max_steps) {
            setState(AgentState::ERROR);
            final_output = "Max steps reached without completion.";
        }
    }

    context_.printSummary();

    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║  [%s] Task Complete ✅               \n", config_.name.c_str());
    printf("╚══════════════════════════════════════════╝\n\n");

    return final_output;
}

} // namespace coreagent