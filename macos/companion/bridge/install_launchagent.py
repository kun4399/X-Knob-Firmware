"""Preview, install, or remove the per-user X-Knob Companion LaunchAgent."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import plistlib
import subprocess

LABEL = "local.xknob.companion"
TARGET = Path.home() / "Library" / "LaunchAgents" / f"{LABEL}.plist"


def definition() -> dict:
    root = Path(__file__).resolve().parents[1]
    state = Path.home() / ".local" / "state" / "xknob-companion"
    return {
        "Label": LABEL,
        "ProgramArguments": [str(root / "examples" / "run_companion.sh")],
        "WorkingDirectory": str(root),
        "RunAtLoad": True,
        "KeepAlive": {"SuccessfulExit": False},
        "StandardOutPath": str(state / "stdout.log"),
        "StandardErrorPath": str(state / "stderr.log"),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--install", action="store_true")
    action.add_argument("--uninstall", action="store_true")
    args = parser.parse_args()
    domain = f"gui/{os.getuid()}"
    if args.uninstall:
        if TARGET.exists():
            subprocess.run(["launchctl", "bootout", domain, str(TARGET)], check=False)
            TARGET.unlink()
        return
    content = plistlib.dumps(definition())
    if not args.install:
        print(content.decode("utf-8"))
        return
    root = Path(__file__).resolve().parents[1]
    if not (root / ".venv" / "bin" / "python").exists():
        raise SystemExit("Create .venv in the Mac Companion directory and install requirements first")
    config = Path.home() / ".config" / "xknob-companion" / "companion.env"
    if not config.exists():
        raise SystemExit(f"Create private config at {config} first")
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    Path(definition()["StandardOutPath"]).parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    if TARGET.exists():
        if TARGET.read_bytes() != content:
            raise SystemExit(f"Existing LaunchAgent differs: {TARGET}; review it before replacing")
        subprocess.run(["launchctl", "bootout", domain, str(TARGET)], check=False)
    TARGET.write_bytes(content)
    TARGET.chmod(0o600)
    subprocess.run(["launchctl", "bootstrap", domain, str(TARGET)], check=True)
    print(f"Installed {TARGET}; remove with --uninstall")


if __name__ == "__main__":
    main()
