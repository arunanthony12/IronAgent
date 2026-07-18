"""
CoreAgent — Bare-Metal AI Agent Runtime
C + C++ + Python
"""

# Import compiled C++ extension
try:
    from ._coreagent_cpp import (
        Agent         as _CppAgent,
        AgentConfig,
        AgentState,
        ToolInput,
        ToolOutput,
        ToolRegistry,
        ContextBuffer,
        MessageRole,
        version,
        state_name,
    )
    _BACKEND_AVAILABLE = True
except ImportError as e:
    _BACKEND_AVAILABLE = False
    _IMPORT_ERROR = str(e)

from .agent import Agent
from .tools import (
    shell_tool,
    read_file_tool,
    write_file_tool,
    http_get_tool,
)

__version__ = "0.1.0"
__author__  = "CoreAgent Contributors"

__all__ = [
    # Main user-facing class
    "Agent",

    # C++ types (advanced users)
    "AgentConfig",
    "AgentState",
    "ToolInput",
    "ToolOutput",
    "ToolRegistry",
    "ContextBuffer",
    "MessageRole",

    # Built-in tools
    "shell_tool",
    "read_file_tool",
    "write_file_tool",
    "http_get_tool",

    # Helpers
    "version",
    "state_name",
]