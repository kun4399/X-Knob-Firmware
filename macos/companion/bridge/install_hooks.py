"""Print or explicitly install a user-level Codex hooks.json config.

Use --install only after reading docs/trust prompts. Preserve existing hooks.
Never silently overwrite someone else's configuration.
"""
import argparse
import copy
import json
from pathlib import Path
import shlex
import sys

EVENTS = ["SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse",
          "PermissionRequest", "Stop", "Interrupt", "SessionEnd"]


def hook_block(event: str, script: Path, python_bin: str) -> dict:
    cmd = shlex.quote(python_bin) + " " + shlex.quote(str(script))
    return {"hooks": [{"type": "command", "command": cmd, "timeout": 2}]}


def build_config(existing: dict, script: Path, python_bin: str) -> dict:
    data = copy.deepcopy(existing)
    data.setdefault("hooks", {})
    for ev in EVENTS:
        grp = data["hooks"].setdefault(ev, [])
        block = hook_block(ev, script, python_bin)
        if not any(g.get("hooks") == block["hooks"] for g in grp if isinstance(g, dict)):
            grp.append(block)
    return data


def remove_config(existing: dict, script: Path, python_bin: str) -> dict:
    data = copy.deepcopy(existing)
    groups = data.get("hooks", {})
    for ev in EVENTS:
        block = hook_block(ev, script, python_bin)
        if ev in groups:
            groups[ev] = [g for g in groups[ev] if not (isinstance(g, dict) and g.get("hooks") == block["hooks"])]
            if not groups[ev]:
                del groups[ev]
    return data


def main():
    parser = argparse.ArgumentParser()
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--install", action="store_true", help="Write ~/.codex/hooks.json; review first!")
    action.add_argument("--uninstall", action="store_true", help="Remove only X-Knob hooks")
    parser.add_argument("--config", type=Path, default=Path.home()/".codex"/"hooks.json")
    args = parser.parse_args()
    script = Path(__file__).resolve().parent / "hook_sender.py"
    config = args.config.expanduser()
    try:
        existing = json.loads(config.read_text(encoding="utf-8")) if config.exists() else {}
    except json.JSONDecodeError:
        raise SystemExit("Refusing to overwrite existing invalid JSON config")
    generated = remove_config(existing, script, sys.executable) if args.uninstall else build_config(existing, script, sys.executable)
    result = json.dumps(generated, indent=2, ensure_ascii=False) + "\n"
    if not args.install and not args.uninstall:
        print(result)
        print("Run again with --install only after review.", file=sys.stderr)
        return
    config.parent.mkdir(parents=True, exist_ok=True)
    if config.exists() and not args.uninstall:
        backup = config.with_suffix(config.suffix+".bak")
        if backup.exists():
            raise SystemExit(f"Backup already exists: {backup}")
        backup.write_bytes(config.read_bytes())
    config.write_text(result, encoding="utf-8")
    config.chmod(0o600)
    print(f"Updated {config}; restart Codex clients and review hook trust prompt")


if __name__ == "__main__":
    main()
