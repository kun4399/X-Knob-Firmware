"""Read Codex account quota and token-activity summaries from app-server.

This launches a NEW Codex app-server child to query account-level metadata.
It does not subscribe to, or control, existing Codex Desktop task streams.
Availability depends on Codex version and authenticated account mode.
"""
from __future__ import annotations
from datetime import datetime, timezone
import json
import os
import selectors
import shutil
import subprocess
import sys
import time


def normalize_rate(result: dict | None):
    if not isinstance(result, dict):
        return []
    rows = result.get("rateLimitsByLimitId") or {}
    if not rows and isinstance(result.get("rateLimits"), dict):
        bucket = result["rateLimits"]
        rows = {str(bucket.get("limitId", "default")): bucket}
    out = []
    for name, value in rows.items():
        if not isinstance(value, dict):
            continue
        windows = []
        for label in ("primary", "secondary"):
            w = value.get(label)
            if not isinstance(w, dict):
                continue
            used = w.get("usedPercent")
            windows.append({
                "window": label,
                "used_percent": float(used) if isinstance(used, (float, int)) else None,
                "remaining_percent": round(max(0.0, min(100.0, 100-float(used))), 2) if isinstance(used, (float, int)) else None,
                "duration_min": w.get("windowDurationMins"),
                "resets_at_unix_s": w.get("resetsAt"),
            })
        out.append({"limit_id": name, "display_name": value.get("limitName") or name,
                    "rate_limit_reached_type": value.get("rateLimitReachedType"), "windows": windows})
    return out


def normalize_usage(result: dict | None):
    if not isinstance(result, dict):
        return None
    s = result.get("summary") or {}
    buckets = result.get("dailyUsageBuckets")
    return {
        "lifetime_tokens": s.get("lifetimeTokens"),
        "peak_daily_tokens": s.get("peakDailyTokens"),
        "daily_buckets": [{"day": x.get("startDate"), "tokens": x.get("tokens")}
                          for x in buckets[-30:] if isinstance(x, dict)] if isinstance(buckets, list) else None,
    }


def query_account(timeout_s=8.0):
    exe = os.environ.get("XKNOB_CODEX_BIN") or shutil.which("codex")
    if not exe:
        return {"quality": "unavailable", "reason": "codex_executable_not_found"}
    args = [exe, "app-server", "--listen", "stdio://"]
    try:
        proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, bufsize=0)
    except OSError:
        return {"quality": "unavailable", "reason": "app_server_launch_failed"}
    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)
    pending = {}
    buffer = b""

    def send(msg):
        proc.stdin.write((json.dumps(msg, ensure_ascii=False)+"\n").encode("utf-8"))
        proc.stdin.flush()

    def await_response(target_id, deadline):
        nonlocal buffer
        if target_id in pending:
            return pending.pop(target_id)
        while time.monotonic() < deadline:
            ready = sel.select(max(0, min(0.3, deadline - time.monotonic())))
            if not ready:
                continue
            data = os.read(proc.stdout.fileno(), 65536)
            if not data:
                raise OSError("codex app-server terminated")
            buffer += data
            if len(buffer) > 4*1024*1024:
                raise ValueError("rpc line buffer over limit")
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                try:
                    m = json.loads(line)
                except (ValueError, UnicodeError):
                    continue
                if "id" in m:
                    pending[m["id"]] = m
            if target_id in pending:
                return pending.pop(target_id)
        raise TimeoutError("codex account query timeout")

    try:
        deadline = time.monotonic() + timeout_s
        send({"method": "initialize", "id": 1,
              "params": {"clientInfo": {"name": "xknob_mac_companion", "title": "X-Knob Companion", "version": "0.1.0"}}})
        init = await_response(1, deadline)
        if "error" in init:
            return {"quality": "unavailable", "reason": "initialize_rejected"}
        send({"method": "initialized", "params": {}})
        send({"method": "account/rateLimits/read", "id": 2})
        send({"method": "account/usage/read", "id": 3})
        rate_resp = await_response(2, deadline)
        usage_resp = await_response(3, deadline)
        rate_ok = isinstance(rate_resp.get("result"), dict)
        usage_ok = isinstance(usage_resp.get("result"), dict)
        return {
            "v": 1,
            "source": "separate_codex_app_server_account_queries",
            "quality": "observed" if (rate_ok or usage_ok) else "unavailable",
            "observed_at": datetime.now(timezone.utc).isoformat(),
            "rate_limits": normalize_rate(rate_resp.get("result")) if rate_ok else None,
            "token_activity": normalize_usage(usage_resp.get("result")) if usage_ok else None,
            "rate_error": None if rate_ok else "rate_limits_unavailable",
            "usage_error": None if usage_ok else "usage_unavailable",
        }
    except (OSError, TimeoutError, ValueError, BrokenPipeError):
        return {"quality": "unavailable", "reason": "query_failed_or_timed_out"}
    finally:
        try:
            sel.close()
            proc.terminate()
            proc.wait(timeout=1)
        except Exception:
            proc.kill()
            try:
                proc.wait(timeout=1)
            except Exception:
                pass
        finally:
            if proc.stdin and not proc.stdin.closed:
                proc.stdin.close()
            if proc.stdout and not proc.stdout.closed:
                proc.stdout.close()


if __name__ == "__main__":
    print(json.dumps(query_account(), indent=2, ensure_ascii=False))
