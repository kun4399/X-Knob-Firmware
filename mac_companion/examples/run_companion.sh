#!/bin/sh
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CFG_DIR="$HOME/.config/xknob-companion"
ENV_FILE="$CFG_DIR/companion.env"
if [ ! -f "$ENV_FILE" ]; then
  echo "Create $ENV_FILE from examples/companion.env.example and protect it (chmod 600)." >&2
  exit 2
fi
set -a
. "$ENV_FILE"
set +a
if [ -z "${XKNOB_MQTT_USER:-}" ]; then
  echo "Set XKNOB_MQTT_USER in $ENV_FILE." >&2
  exit 2
fi
# The password is kept in the user's login Keychain, never in the plist or env file.
XKNOB_MQTT_PASSWORD=$(/usr/bin/security find-generic-password -s local.xknob.mqtt -a "$XKNOB_MQTT_USER" -w)
export XKNOB_MQTT_PASSWORD
cd "$APP_DIR"
exec "$APP_DIR/.venv/bin/python" -m bridge.service
