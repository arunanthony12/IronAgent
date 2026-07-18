#pragma once

#include "AgentState.hpp"
#include "ToolRegistry.hpp"
#include "ContextBuffer.hpp"
#include "Arena.hpp"
#include "Executor.hpp"
#include "ThreadPool.hpp"

#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace coreagent {

/* ─────────────────────────────────────────────────────────
 * AgentConfig — all agent settings in one place
 * ───────────────────────────────────────────────────────── */
struct AgentConfig {
    std::string name          = "CoreAgent";
    std::string system_prompt = "You are a fast, efficient AI agent. "
                                "Use available tools to complete tasks.";
    size_t      max_tokens    = 4096;
    uint32_t    num_threads   = 4;
    uint32_t    max_steps     = 10;
    uint32_t    tool_timeout  = 10000;  // ms

    // ContextBuffer sizing (v1.1): retained_/standby_ each get
    // context_arena_capacity bytes; scratch_ (buildPrompt output) gets
    // context_scratch_capacity. Previous default of 1MB (CA_ARENA_DEFAULT_SIZE)
    // was too small for sustained loops and caused constant compaction --
    // see ContextBuffer.hpp's compact() for why FIFO eviction needs headroom.
    size_t      context_arena_capacity   = 4 * 1024 * 1024;  // 4MB
    size_t      context_scratch_capacity = 256 * 1024;       // 256KB
};

/* ─────────────────────────────────────────────────────────
 * Agent — core state machine
 *
 *  IDLE → THINKING → PLANNING → ACTING → REFLECTING
 *            ↑___________________________________|
 *                   (loop until DONE/ERROR)
 * ───────────────────────────────────────────────────────── */
class Agent {
public:
    explicit Agent(const AgentConfig& config = AgentConfig{});
    ~Agent() = default;

    /* Non-copyable, movable */
    Agent(const Agent&)            = delete;
    Agent& operator=(const Agent&) = delete;

    /* Register a custom tool */
    void registerTool(const std::string& name,
                      const std::string& description,
                      ToolFn fn);

    /* Run agent on a task — blocks until DONE or max_steps */
    std::string run(const std::string& task);

    /* Accessors */
    AgentState    state()     const { return state_;   }
    std::string   stateName() const { return agentStateToString(state_); }
    std::string   name()      const { return config_.name; }

    ContextBuffer& context() { return context_; }
    ToolRegistry&  tools()   { return registry_; }

private:
    AgentConfig   config_;
    AgentState    state_;
    ContextBuffer context_;
    ToolRegistry  registry_;
    ThreadPool    thread_pool_;

    /* ── Agent loop steps ──────────────────────────────── */
    std::string think  (const std::string& task);
    std::string plan   (const std::string& thought);
    ToolOutput  act    (const std::string& plan_str);
    bool        reflect(const ToolOutput& result,
                        std::string& final_output);

    void setState(AgentState s);
    void setupBuiltinTools();
};

} // namespace coreagent