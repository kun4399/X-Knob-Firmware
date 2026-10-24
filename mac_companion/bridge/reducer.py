"""Pure event reducer: no external dependencies or personal content.

Hook events are not exact copies of Codex turn outcome. A Stop hook tells us a
turn stopped, but not that every underlying user goal succeeded.
"""
from __future__ import annotations
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any
import hashlib
import secrets
import time

KNOWN = {
    "SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse",
    "PermissionRequest", "Stop", "Interrupt", "SessionEnd"
}


def public_session_id(raw: str) -> str:
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:12]


@dataclass
class Session:
    sid: str
    phase: str = "idle"
    turn: str = ""
    last_seen: float = field(default_factory=time.time)
    pending_since: float | None = None
    last_change: float = field(default_factory=time.time)
    last_event: str = ""


class Reducer:
    def __init__(self, host_id: str = "example-host"):
        self.host_id = host_id
        self.boot_id = secrets.token_hex(4)
        self.sessions: dict[str, Session] = {}
        self.seq = 0
        self.last_published: dict[str, Any] = {}

    def ingest(self, event: dict, at: float | None = None) -> dict | None:
        name = str(event.get("hook_event_name", ""))
        if name not in KNOWN:
            return None
        raw_sid = str(event.get("session_id", ""))
        if not raw_sid:
            return None
        now = time.time() if at is None else at
        sid = public_session_id(raw_sid)
        s = self.sessions.setdefault(sid, Session(sid=sid, last_seen=now, last_change=now))
        previous_phase = s.phase
        s.last_seen = now
        s.turn = public_session_id(str(event.get("turn_id", "unknown")))
        s.last_event = name
        transition = {
            "SessionStart": "idle",
            "UserPromptSubmit": "running",
            "PreToolUse": "running",
            "PostToolUse": "running",
            "PermissionRequest": "approval_pending",
            "Stop": "turn_stopped_unverified",
            "Interrupt": "interrupted",
            "SessionEnd": "closed",
        }[name]
        if name == "SessionStart" and previous_phase not in ("idle", "closed"):
            transition = previous_phase
        if transition != previous_phase:
            s.last_change = now
        s.phase = transition
        if name == "PermissionRequest":
            s.pending_since = now
        if name in ("PostToolUse", "Stop", "Interrupt", "SessionEnd"):
            s.pending_since = None
        self.seq += 1
        event_type = {
            "UserPromptSubmit": "codex.turn.started",
            "PermissionRequest": "codex.approval.requested",
            "Stop": "codex.turn.stopped",
            "Interrupt": "codex.turn.interrupted",
            "SessionEnd": "codex.thread.ended",
        }.get(name)
        if event_type:
            return {
                "v": 1, "event_id": f"{self.host_id}-{self.boot_id}-{self.seq}",
                "boot_id": self.boot_id,
                "seq": self.seq, "kind": event_type, "source": "codex_hooks",
                "host_id": self.host_id, "session_ref": sid,
                "ts": datetime.fromtimestamp(now, timezone.utc).isoformat(),
                "outcome_verified": False,
            }
        return None

    def snapshot(self, at: float | None = None) -> dict:
        now = time.time() if at is None else at
        last_observed = max((s.last_seen for s in self.sessions.values()), default=None)
        sessions = []
        for s in self.sessions.values():
            phase = s.phase
            # Timeout is a stale-data marker, not proof of a finished turn.
            if now - s.last_seen > 600 and phase in ("running", "approval_pending"):
                phase = "stale_unknown"
            sessions.append({"session_ref": s.sid, "phase": phase, "age_s": round(now - s.last_seen, 1)})
        priority = {"approval_pending": 0, "running": 1, "stale_unknown": 2,
                    "interrupted": 3, "turn_stopped_unverified": 4, "idle": 5, "closed": 6}
        sessions.sort(key=lambda x: (priority.get(x["phase"], 9), x["age_s"]))
        focus = sessions[0] if sessions else None
        return {
            "v": 1, "host_id": self.host_id, "boot_id": self.boot_id,
            "observed_at": datetime.fromtimestamp(last_observed, timezone.utc).isoformat() if last_observed else None,
            "source": "codex_hooks", "quality": "unknown" if last_observed is None else
                ("stale" if now - last_observed > 600 else "observed"),
            "focused_session_ref": focus["session_ref"] if focus else None,
            "focused_phase": focus["phase"] if focus else "unknown",
            "running_count": sum(x["phase"] == "running" for x in sessions),
            "approval_count": sum(x["phase"] == "approval_pending" for x in sessions),
            "sessions": sessions[:8]
        }
