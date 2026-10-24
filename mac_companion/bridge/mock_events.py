"""Emit an example sequence to a running local companion, for smoke testing.

No Codex prompts or user content are used.
"""
from .hook_sender import forward
import time

sid = "local-simulation-session"
for name in ["SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "PermissionRequest", "PostToolUse", "Stop"]:
    ok = forward({"hook_event_name": name, "session_id": sid, "turn_id": "mock-turn-1"})
    print(f"{name}: {'sent' if ok else 'not sent'}")
    time.sleep(0.25)
