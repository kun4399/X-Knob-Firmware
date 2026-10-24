"""Local ingress + MQTT publisher proof of concept for macOS.

Run from the Mac user session, not as root. Dependencies: paho-mqtt >= 2.
The server listens on loopback ONLY and authenticates every POST.
"""
from __future__ import annotations
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import os
import secrets
import threading
import time
from .reducer import Reducer
from .account_probe import query_account
from .tunnel import SshTunnel
import signal

BASE = Path.home() / ".config" / "xknob-companion"


def secret_key() -> str:
    BASE.mkdir(mode=0o700, parents=True, exist_ok=True)
    f = BASE / "bridge.key"
    if not f.exists():
        flags = os.O_CREAT | os.O_WRONLY | os.O_EXCL
        fd = os.open(f, flags, 0o600)
        with os.fdopen(fd, "w", encoding="ascii") as out:
            out.write(secrets.token_hex(32))
    return f.read_text(encoding="ascii").strip()


class Publisher:
    def __init__(self, host: str, client_id: str):
        import paho.mqtt.client as mqtt
        self.mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        user = os.environ.get("XKNOB_MQTT_USER")
        if user:
            self.mqtt.username_pw_set(user, os.environ.get("XKNOB_MQTT_PASSWORD", ""))
        # Configure TLS for production before enabling access beyond trusted LAN.
        self.mqtt.will_set(f"xknob/v1/codex/{host}/availability", "offline", 1, retain=True)
        self.mqtt.on_connect = self.on_connect
        self.host_id = host
        self.retain_cache = {}
        self.cache_lock = threading.RLock()
        self.mqtt.connect_async(os.environ.get("XKNOB_MQTT_HOST", "127.0.0.1"),
                                int(os.environ.get("XKNOB_MQTT_PORT", "1883")))
        self.mqtt.loop_start()

    def on_connect(self, client, userdata, flags, reason_code, properties):
        if not reason_code.is_failure:
            client.publish(f"xknob/v1/codex/{self.host_id}/availability", "online", qos=1, retain=True)
            with self.cache_lock:
                if not self.retain_cache:
                    unknown = {"v": 1, "host_id": self.host_id, "quality": "unknown",
                               "focused_phase": "unknown", "observed_at": None, "source": "codex_hooks"}
                    self.retain_cache["status"] = unknown
                cached = list(self.retain_cache.items())
            for kind, value in cached:
                client.publish(f"xknob/v1/codex/{self.host_id}/{kind}",
                               json.dumps(value, ensure_ascii=False), qos=1, retain=True)

    def publish(self, kind: str, data: dict, retain: bool):
        topic = f"xknob/v1/codex/{self.host_id}/{kind}"
        if retain:
            with self.cache_lock:
                self.retain_cache[kind] = data
        self.mqtt.publish(topic, json.dumps(data, ensure_ascii=False, separators=(",", ":")),
                          qos=1, retain=retain)


class Service:
    def __init__(self, host: str):
        self.host = host
        self.key = secret_key()
        self.publisher = Publisher(host, f"xknob-mac-{host}")
        self.reducer = Reducer(host)
        self.lock = threading.Lock()
        self.events_seen = 0
        self.account_last_quality = "not_started"

    def account_loop(self):
        while True:
            summary = query_account(timeout_s=8.0)
            summary["host_id"] = self.host
            self.account_last_quality = summary.get("quality", "unavailable")
            # Keep MQTT payload within the knob's bounded parser buffer.
            limits = summary.get("rate_limits") or []
            chosen = next((x for x in limits if x.get("limit_id") == "codex"), limits[0] if limits else None)
            activity = summary.get("token_activity") or {}
            daily_tokens = [
                max(0, int(bucket.get("tokens") or 0))
                for bucket in (activity.get("daily_buckets") or [])[-7:]
                if isinstance(bucket, dict)
            ]
            compact = {"v": 1, "host_id": self.host, "quality": summary.get("quality", "unavailable"),
                       "source": summary.get("source"), "observed_at": summary.get("observed_at"),
                       "rate_limits": [chosen] if chosen else [],
                       "token_activity": {"lifetime_tokens": activity.get("lifetime_tokens"),
                                          "daily_tokens": daily_tokens}}
            self.publisher.publish("usage", compact, retain=True)
            time.sleep(max(120, int(os.environ.get("XKNOB_ACCOUNT_POLL_SEC", "300"))))

    def status_loop(self):
        while True:
            time.sleep(30)
            with self.lock:
                self.publisher.publish("status", self.reducer.snapshot(), retain=True)

    def process(self, payload: dict):
        with self.lock:
            event = self.reducer.ingest(payload)
            snapshot = self.reducer.snapshot()
            self.publisher.publish("status", snapshot, retain=True)
            if event:
                self.publisher.publish("event", event, retain=False)
            self.events_seen += 1


def make_handler(service: Service):
    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            if self.path != "/v1/hook" or self.headers.get("X-Bridge-Key", "") != service.key:
                self.send_error(403)
                return
            try:
                n = int(self.headers.get("Content-Length", "0"))
                if n <= 0 or n > 4096:
                    self.send_error(413)
                    return
                event = json.loads(self.rfile.read(n))
                if not isinstance(event, dict):
                    raise ValueError("object expected")
                service.process(event)
                self.send_response(204)
                self.end_headers()
            except Exception:
                self.send_error(400)

        def do_GET(self):
            if self.path != "/healthz":
                self.send_error(404)
                return
            body = json.dumps({"ok": True, "events": service.events_seen}).encode("ascii")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt, *args):
            pass
    return Handler


def main():
    host_id = os.environ.get("XKNOB_MAC_ID", "example-host")
    tunnel_target = os.environ.get("XKNOB_SSH_TUNNEL_TARGET", "")
    tunnel = SshTunnel(tunnel_target, int(os.environ.get("XKNOB_MQTT_PORT", "18830"))) if tunnel_target else None
    if tunnel:
        tunnel.start()
    s = Service(host_id)
    server = ThreadingHTTPServer(("127.0.0.1", 17345), make_handler(s))
    if os.environ.get("XKNOB_ENABLE_ACCOUNT_POLL", "0") == "1":
        threading.Thread(target=s.account_loop, name="codex-account-poll", daemon=True).start()
    threading.Thread(target=s.status_loop, name="codex-status-age", daemon=True).start()
    print("XKnob Companion: localhost:17345, publishing to MQTT", flush=True)
    if tunnel:
        def stop_on_term(*_):
            raise SystemExit(0)
        signal.signal(signal.SIGTERM, stop_on_term)
    try:
        server.serve_forever(poll_interval=0.25)
    finally:
        server.server_close()
        if tunnel:
            tunnel.stop()


if __name__ == "__main__":
    main()
