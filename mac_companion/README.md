# Mac Companion

This Mac service converts Codex lifecycle hooks to small, content-free MQTT
status messages. `Stop` means **this turn ended**, not verified success.

## Setup

```sh
cd mac_companion
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

## Start the bridge manually

Copy `examples/companion.env.example` to
`~/.config/xknob-companion/companion.env` (mode 0600). The example uses an SSH
forward to the existing NAS broker, avoiding a cleartext MQTT password over a
public route. The SSH target must exist in the Mac user's SSH config. Keep the
MQTT password in macOS Keychain. The `security` command below prompts in an
interactive terminal; confirm that the saved item is nonempty before starting:

```sh
security add-generic-password -U -s local.xknob.mqtt -a mac_companion -w
```

Start through `examples/run_companion.sh`. It reads the Keychain password without
putting it in the config file or LaunchAgent plist.

The service listens ONLY on 127.0.0.1:17345. On first run it writes a
random local bridge key to ~/.config/xknob-companion/bridge.key (0600).

In another terminal:

```sh
curl http://127.0.0.1:17345/healthz
.venv/bin/python -m bridge.mock_events
```

Install hooks only after reviewing the proposed config:

```sh
.venv/bin/python -m bridge.install_hooks
.venv/bin/python -m bridge.install_hooks --install
```

Remove only these hooks with `.venv/bin/python -m bridge.install_hooks --uninstall`.

## Account metadata

```sh
.venv/bin/python -m bridge.account_probe
```

If supported and authenticated, enable separate account polling in the private
environment file:

```sh
XKNOB_ENABLE_ACCOUNT_POLL=1
XKNOB_ACCOUNT_POLL_SEC=300
```

The app-server account endpoint does not observe Codex Desktop's active
thread events. Desktop activity uses lifecycle hooks. If the account endpoint
omits data, the display says unavailable rather than treating missing values
as zero.

## LaunchAgent on macOS

1. Copy `examples/companion.env.example` to the private config path, set the
   host ID to match `XK_CODEX_HOST_ID` in the knob's local `src/config.h`, edit
   the SSH target/user, and set mode 0600.
2. Add the MQTT password to Keychain using the interactive `security` command above.
3. Preview, then install the per-user service:

```sh
.venv/bin/python -m bridge.install_launchagent
.venv/bin/python -m bridge.install_launchagent --install
```

Use `.venv/bin/python -m bridge.install_launchagent --uninstall` for removal.
Do not store secrets in Git, plist, or world-readable logs.

## Current verification and limits

- Eight host tests passed, including fake app-server and synthetic lifecycle
  events. The Mac LaunchAgent and user-level hooks were installed locally.
- The configured MQTT account was checked against its topic ACL, including a
  denied attempt to publish to the light command topic. Keep the actual broker
  hostname, account name, host ID, and SSH alias in local configuration only.
- The LaunchAgent health endpoint returned OK. Retained broker status showed
  `availability=online`, `status.quality=unknown` without real hook events, and
  `usage.quality=observed` from a read-only account probe.
- Codex must load and trust the newly installed hook configuration. A real
  Codex event reaching the physical knob has not yet been verified.

## Repository boundary

This directory is the complete macOS-side component. Its Python runtime,
dependencies, hooks, local service, tests, and optional LaunchAgent are
maintained independently from `src/` and `platformio.ini`. A firmware build
does not install or start this service. Runtime configuration belongs under
`~/.config/xknob-companion/`, and credentials belong in Keychain; do not copy
either into this repository.
