"""
shared_memory.py - A simple thread-safe blackboard that multiple
Agent instances can read from and write to (Phase 6c).

Kept deliberately simple: a key -> value store, tagged with who wrote
each entry. Agents that join the same SharedMemory automatically see
its contents surfaced into their planning context on each run_llm()
call, via Agent.run_llm()'s auto-injection.
"""

import threading
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List


@dataclass
class MemoryEntry:
    value: Any
    written_by: str
    timestamp: float = field(default_factory=time.time)


class SharedMemory:
    """A simple key-value blackboard shared across a team of agents."""

    def __init__(self):
        self._data: Dict[str, MemoryEntry] = {}
        self._lock = threading.Lock()

    def write(self, key: str, value: Any, by: str = "unknown") -> None:
        with self._lock:
            self._data[key] = MemoryEntry(value=value, written_by=by)

    def read(self, key: str, default: Any = None) -> Any:
        with self._lock:
            entry = self._data.get(key)
            return entry.value if entry else default

    def read_all(self) -> Dict[str, Any]:
        with self._lock:
            return {k: e.value for k, e in self._data.items()}

    def keys(self) -> List[str]:
        with self._lock:
            return list(self._data.keys())

    def summary(self) -> str:
        """Human-readable dump, meant to be injected into an agent's prompt."""
        with self._lock:
            if not self._data:
                return ""
            lines = [
                f"- {k} = {e.value!r} (written by {e.written_by})"
                for k, e in self._data.items()
            ]
            return "\n".join(lines)
