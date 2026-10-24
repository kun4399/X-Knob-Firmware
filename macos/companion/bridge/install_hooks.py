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
import plistlib

# Battery-first telemetry: only events with user-visible meaning by default.
# Advanced users can opt in to per-tool hooks, at a real process-launch cost.
EVENTS = ["SessionStart", "UserPromptSubmit", "PermissionRequest",
          "Stop", "Interrupt", "SessionEnd"]
TOOL_EVENTS = ["PreToolUse", "PostToolUse"]
APP_ID = "local.xknob.companion.app"


def owned_command(command: str, scripts=()) -> bool:
    """Match known project layouts, never a generic unrelated hook_sender.py."""
    try:
        argv = shlex.split(command)
    except (ValueError, TypeError):
        return False
    if len(argv) == 2 and argv[1] == "--hook":
        exe = Path(argv[0]).expanduser()
        if exe.name == "XKnobCompanion" and exe.parent.name == "MacOS":
            try:
                return plistlib.loads((exe.parent.parent/"Info.plist").read_bytes()).get("CFBundleIdentifier") == APP_ID
            except OSError:
                # A moved/deleted copy leaves no plist to inspect. Recognize
                # only our exact shipped app name, not arbitrary app helpers.
                return exe.parent.parent.parent.name == "X-Knob Companion.app"
            except ValueError:
                return False
    if len(argv) != 2:
        return False
    target = Path(argv[1]).expanduser().resolve()
    if target in {Path(p).expanduser().resolve() for p in scripts}:
        return True
    suffixes = [
        ("X-Knob", "1.Firmware", "mac_companion", "bridge", "hook_sender.py"),
        ("X-Knob", "macos", "companion", "bridge", "hook_sender.py"),
        ("X-Knob", "macos", "bridge", "hook_sender.py"),
    ]
    return any(target.parts[-len(s):] == s for s in suffixes)


def clean_project_hooks(existing: dict, scripts=(), stale_only=False) -> dict:
    data = copy.deepcopy(existing)
    for event, groups in list(data.get("hooks", {}).items()):
        if event == "state" or not isinstance(groups, list):
            continue
        kept = []
        for group in groups:
            if not isinstance(group, dict) or not isinstance(group.get("hooks"), list):
                kept.append(group)
                continue
            hooks = []
            for hook in group["hooks"]:
                ours = isinstance(hook, dict) and hook.get("type") == "command" and owned_command(hook.get("command", ""), scripts)
                stale = False
                if ours:
                    argv = shlex.split(hook["command"])
                    stale = not Path(argv[0]).expanduser().exists() or (argv[1] != "--hook" and not Path(argv[1]).expanduser().exists())
                if not ours or (stale_only and not stale):
                    hooks.append(hook)
            if hooks or not group["hooks"]:
                group["hooks"] = hooks
                kept.append(group)
        if kept:
            data["hooks"][event] = kept
        else:
            del data["hooks"][event]
    return data


def build_app_config(existing: dict, executable: Path) -> dict:
    data = clean_project_hooks(existing)
    for event in EVENTS:
        data.setdefault("hooks", {}).setdefault(event, []).append({"hooks": [{
            "type": "command", "command": shlex.quote(str(executable)) + " --hook", "timeout": 2
        }]})
    return data


def hook_block(event: str, script: Path, python_bin: str) -> dict:
    cmd = shlex.quote(python_bin) + " " + shlex.quote(str(script))
    return {"hooks": [{"type": "command", "command": cmd, "timeout": 2}]}


def build_config(existing: dict, script: Path, python_bin: str,
                 include_tools: bool = False) -> dict:
    # Replace only OUR previously installed hooks; otherwise a V2→V3 upgrade
    # would leave noisy Pre/PostToolUse processes running forever.
    data = remove_config(existing, script, python_bin)
    data.setdefault("hooks", {})
    for ev in EVENTS + (TOOL_EVENTS if include_tools else []):
        grp = data["hooks"].setdefault(ev, [])
        block = hook_block(ev, script, python_bin)
        if not any(g.get("hooks") == block["hooks"] for g in grp if isinstance(g, dict)):
            grp.append(block)
    return data


def remove_config(existing: dict, script: Path, python_bin: str) -> dict:
    data = copy.deepcopy(existing)
    groups = data.get("hooks", {})
    owned_paths = {script.expanduser().resolve()}
    if script.parent.name == "bridge" and script.parent.parent.name == "companion":
        # Before the repository split, this same sender lived at macos/bridge/.
        owned_paths.add((script.parent.parent.parent / "bridge" / script.name).resolve())

    def owned(hook):
        if not isinstance(hook, dict) or hook.get("type") != "command":
            return False
        try:
            argv = shlex.split(hook.get("command", ""))
        except (ValueError, TypeError):
            return False
        # Recognize our sender despite a venv interpreter/version change. Do
        # not remove similarly named scripts in another project or shell chains.
        return owned_command(hook.get("command", ""), owned_paths)

    for ev in EVENTS + TOOL_EVENTS:
        if ev in groups:
            kept = []
            for group in groups[ev]:
                if not isinstance(group, dict) or not isinstance(group.get("hooks"), list):
                    kept.append(group)
                    continue
                hooks = group["hooks"]
                remaining = [h for h in hooks if not owned(h)]
                if remaining or not hooks:
                    group["hooks"] = remaining
                    kept.append(group)
            groups[ev] = kept
            if not groups[ev]:
                del groups[ev]
    return data


def main():
    parser = argparse.ArgumentParser()
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--install", action="store_true", help="Write ~/.codex/hooks.json; review first!")
    action.add_argument("--uninstall", action="store_true", help="Remove only X-Knob hooks")
    parser.add_argument("--config", type=Path, default=Path.home()/".codex"/"hooks.json")
    parser.add_argument("--include-tools", action="store_true",
                        help="Opt in to high-frequency per-tool telemetry")
    args = parser.parse_args()
    script = Path(__file__).resolve().parent / "hook_sender.py"
    config = args.config.expanduser()
    try:
        existing = json.loads(config.read_text(encoding="utf-8")) if config.exists() else {}
    except json.JSONDecodeError:
        raise SystemExit("Refusing to overwrite existing invalid JSON config")
    generated = remove_config(existing, script, sys.executable) if args.uninstall else build_config(
        existing, script, sys.executable, include_tools=args.include_tools)
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
