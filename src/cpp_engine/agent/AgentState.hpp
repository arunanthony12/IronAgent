#pragma once

#include <string>

namespace coreagent {

/* ─────────────────────────────────────────────────────────
 * AgentState — all states in the agent's lifecycle
 *
 *  IDLE → THINKING → PLANNING → ACTING → REFLECTING → DONE
 *                                              ↓
 *                                           ERROR
 * ───────────────────────────────────────────────────────── */
enum class AgentState {
    IDLE,        // Waiting for a task
    THINKING,    // Analyzing the task / forming a thought
    PLANNING,    // Deciding which tool to call + args
    ACTING,      // Executing the chosen tool
    REFLECTING,  // Evaluating the result, deciding next step
    DONE,        // Task successfully completed
    ERROR        // Unrecoverable error
};

inline std::string agentStateToString(AgentState state) {
    switch (state) {
        case AgentState::IDLE:       return "IDLE";
        case AgentState::THINKING:   return "THINKING";
        case AgentState::PLANNING:   return "PLANNING";
        case AgentState::ACTING:     return "ACTING";
        case AgentState::REFLECTING: return "REFLECTING";
        case AgentState::DONE:       return "DONE";
        case AgentState::ERROR:      return "ERROR";
        default:                     return "UNKNOWN";
    }
}

} // namespace coreagent