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
