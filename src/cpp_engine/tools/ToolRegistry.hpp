#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cstdint>  

namespace coreagent {

/* ─────────────────────────────────────────────────────────
 * ToolInput / ToolOutput — the interface every tool uses
 * ───────────────────────────────────────────────────────── */
struct ToolInput {
    std::string name;    // Tool name
    std::string args;    // Raw args string (tool parses as needed)
};

struct ToolOutput {
    std::string result;      // Tool output on success
    bool        success;     // Did it succeed?
    std::string error;       // Error message on failure
    uint32_t    elapsed_ms;  // How long it took
};

/* Tool function type */
using ToolFn = std::function<ToolOutput(const ToolInput&)>;

struct ToolDef {
    std::string name;
    std::string description;
    ToolFn      fn;
};

/* ─────────────────────────────────────────────────────────
 * ToolRegistry — register tools, dispatch by name
 * ───────────────────────────────────────────────────────── */
class ToolRegistry {
public:

    void registerTool(const std::string& name,
                      const std::string& description,
                      ToolFn fn) {
        if (tools_.count(name))
            throw std::runtime_error("[ToolRegistry] Already registered: " + name);

        tools_[name] = ToolDef{name, description, fn};
        names_.push_back(name);

        printf("[ToolRegistry] Registered: %-18s → %s\n",
               name.c_str(), description.c_str());
    }

    ToolOutput dispatch(const ToolInput& input) const {
        auto it = tools_.find(input.name);
        if (it == tools_.end())
            return ToolOutput{"", false, "Unknown tool: " + input.name, 0};
        try {
            return it->second.fn(input);
        } catch (const std::exception& e) {
            return ToolOutput{"", false,
                std::string("[ToolRegistry] Exception: ") + e.what(), 0};
        }
    }

    bool hasTool(const std::string& name) const {
        return tools_.count(name) > 0;
    }

    const std::vector<std::string>& toolNames() const { return names_; }
    size_t size() const { return tools_.size(); }

    void printAll() const {
        printf("\n[ToolRegistry] Available tools (%zu):\n", tools_.size());
        for (auto& name : names_) {
            auto& t = tools_.at(name);
            printf("  %-20s : %s\n", t.name.c_str(), t.description.c_str());
        }
    }

private:
    std::unordered_map<std::string, ToolDef> tools_;
    std::vector<std::string>                 names_;
};

} // namespace coreagent