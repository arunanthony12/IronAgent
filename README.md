# ⚙️ IronAgent

![Python 3.10+](https://img.shields.io/badge/Python-3.10%2B-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)
![C11](https://img.shields.io/badge/C-11-282C34.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

> A bare-metal AI agent runtime built from scratch in C, C++, and Python. Features a 2ns custom AVX2-aligned arena allocator, O(1) context sliding windows, and zero JS dependencies. Engineered for hardware-level orchestration.

IronAgent is an experimental, ultra-lightweight AI orchestrator designed to solve the memory bloat and execution overhead of modern agentic frameworks (like LangChain). By pushing the `Think -> Plan -> Act` state machine and context memory down to the native C/C++ layer, it bypasses standard Python garbage collection in favor of a custom 32-byte aligned memory arena. 

It is built to run local LLMs natively alongside shell and filesystem tools, operating entirely without Node.js dependencies.

---

## 🏗️ Architecture

```mermaid
graph TD
    %% Styling
    classDef python fill:#4B8BBE,stroke:#306998,stroke-width:2px,color:white;
    classDef cpp fill:#00599C,stroke:#004482,stroke-width:2px,color:white;
    classDef ccore fill:#282C34,stroke:#61DAFB,stroke-width:2px,color:white;
    classDef boundary fill:#E34F26,stroke:#A1371A,stroke-width:2px,color:white,stroke-dasharray: 5 5;

    %% Nodes
    User((User / CLI))
    LLM{{Ollama qwen2.5:3b}}

    subgraph Python Layer [Python 3.10+ Wrapper]
        API[Agent API]:::python
        MCP[FastMCP Tool Server]:::python
        OClient[HTTP Generation Client]:::python
    end

    subgraph FFI [Pybind11 Boundary]
        Bridge{Deep-Copy Bottleneck}:::boundary
    end

    subgraph CppLayer [C++ Engine]
        State[Think ➔ Plan ➔ Act Loop]:::cpp
        Ctx[ContextBuffer: O1 Window]:::cpp
        Reg[Tool Registry]:::cpp
    end

    subgraph CCore [C Bare-Metal]
        Arena[2ns AVX2 Arena Allocator]:::ccore
        Threads[Pthread Worker Pool]:::ccore
    end

    %% Connections
    User --> API
    API --> OClient
    OClient <--> LLM
    API <--> Bridge
    MCP <--> Bridge

    Bridge <--> State
    Bridge <--> Ctx
    Bridge <--> Reg

    State --> Ctx
    State --> Reg

    Ctx --> Arena
    Reg --> Threads

The 3 Layers:
The C Core (Bare-Metal): A custom AVX2-aligned memory arena (allocator.c) and a lightweight thread pool that execute at hardware speeds (allocations measured in ~2ns).

The C++ Engine: The core state machine and ContextBuffer. Replaces O(N²) array slicing with an O(1) std::deque sliding window, enabling massive token-limit memory prunes in under 0.5ms.

The Python Layer: FastMCP tool routing, the Ollama HTTP wrapper, and user-facing APIs, bridged to the C++ engine via Pybind11.

📊 Benchmarks: The Brutal Truth (v1.0.0)
We benchmarked IronAgent against LangChain using a local qwen2.5:3b model. The results exposed the massive power of our C-core, but also a critical architectural bottleneck.

🏆 The Win: Cold-Start Footprint
Because our orchestration memory is handled natively in C++, IronAgent dominates in cold-start memory usage and initialization speed.

IronAgent: 208 MB (RSS)

LangChain: 424 MB (RSS)

💀 The Loss: The Hot Loop (50,000 Iterations)
In a sustained 50,000-iteration stress test, IronAgent's throughput was completely crushed.

LangChain: 1.8 seconds (27,621 iters/sec)

IronAgent: 41.9 seconds (1,192 iters/sec)

Why did we lose? The Pybind11 Tollbooth.
Our internal C++ engine is blindingly fast, but the Think -> Plan -> Act while-loop currently resides in the Python layer. On every single cycle, massive string payloads must cross the Python ↔ C++ boundary. Pybind11 mandates a blocking deep-copy of these strings, creating a massive I/O bottleneck that renders our internal speed irrelevant. LangChain bypasses this entirely using native C \n.join() operations inside Python.

⚙️ Installation
Prerequisites:

cmake (3.15+)

gcc or clang (C++20 support required)

python (3.10+)

A local ollama instance.

# 1. Clone the repository
git clone [https://github.com/YOUR_USERNAME/IronAgent.git](https://github.com/YOUR_USERNAME/IronAgent.git)
cd IronAgent

# 2. Create a virtual environment
python3 -m venv .venv
source .venv/bin/activate

# 3. Build the C/C++ extensions and install dependencies
pip install -e .

💻 Quick Start
IronAgent is designed to be completely transparent and easy to invoke from Python, while doing all the heavy lifting in C++.

from coreagent.agent import Agent

# 1. Instantiate the bare-metal agent
agent = Agent(name="Jarvis", num_threads=4)

# 2. Inject system constraints into the C++ ContextBuffer
agent.context.add_system(
    "You are an elite, bare-metal AI agent. "
    "Always think step-by-step, plan your tool usage, and act."
)

# 3. Feed it a task
agent.context.add_user("Create a file named 'hello_world.txt' with the text 'IronAgent is alive!'")

# 4. Trigger the C++ / Python orchestrator loop
agent.run_llm(model="qwen2.5:3b")

Adding Custom Tools
You can inject Python tools directly into the C++ ToolRegistry using a simple decorator:

from coreagent import Agent, ToolInput, ToolOutput

agent = Agent()

@agent.tool("multiply", "Multiply two numbers. args='a b'")
def multiply(input: ToolInput) -> ToolOutput:
    out = ToolOutput()
    try:
        a, b = map(int, input.args.split())
        out.result = str(a * b)
        out.success = True
    except Exception as e:
        out.success = False
        out.error = str(e)
    return out

# The agent can now natively plan and call `multiply` during its Act phase.

🚀 Roadmap (v1.1)
To achieve true bare-metal dominance over LangChain, v1.1 will eliminate the FFI overhead:

Zero-Copy Boundaries: Implement std::string_view across Pybind11 to eliminate string deep-copying between Python and C++.

Arena-Backed Buffers: Wire the C++ ContextBuffer directly into our custom ca_arena_t allocator instead of falling back to the OS heap.

Migrate the Hot Loop: Move the core orchestrator loop entirely into C++. Python will strictly be used for initial setup, tool definitions, and final output.

🛡️ License
This project is licensed under the MIT License - see the LICENSE file for details
