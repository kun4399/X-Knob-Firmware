"""Codex hook adapter. Never forward prompts, commands, paths, or tool output.

Keep this program short and fail-open. Codex is always allowed to proceed.
Stop expects JSON on stdout; other hook events may return no stdout.
"""
import json
import os
import pathlib
import sys
import urllib.request

BASE = pathlib.Path.home() / ".config" / "xknob-companion"
ALLOWED = ("hook_event_name", "session_id", "turn_id", "source", "tool_name")


def forward(payload: dict) -> bool:
    try:
        token = (BASE / "bridge.key").read_text(encoding="ascii").strip()
        safe = {k: payload[k] for k in ALLOWED if k in payload and isinstance(payload[k], (str, int))}
        data = json.dumps(safe, separators=(",", ":")).encode("utf-8")
        req = urllib.request.Request(
            "http://127.0.0.1:17345/v1/hook", data=data,
            headers={"Content-Type": "application/json", "X-Bridge-Key": token}, method="POST"
        )
        with urllib.request.urlopen(req, timeout=0.35) as r:
            return r.status == 204
    except Exception:
        return False


def main() -> int:
    try:
        payload = json.load(sys.stdin)
        if isinstance(payload, dict):
            forward(payload)
            if payload.get("hook_event_name") == "Stop":
                print("{}")  # Never block or continue Codex from telemetry.
    except Exception:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
