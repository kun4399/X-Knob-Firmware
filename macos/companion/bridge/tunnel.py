"""Keep a private SSH forward to the NAS broker for the Mac publisher."""
from __future__ import annotations

import subprocess
import threading
import time


class SshTunnel:
    def __init__(self, target: str, local_port: int, remote_port: int = 1883):
        self.target = target
        self.local_port = local_port
        self.remote_port = remote_port
        self.stopping = threading.Event()
        self.process: subprocess.Popen | None = None
        self.thread = threading.Thread(target=self._run, name="mqtt-ssh-tunnel", daemon=True)

    def start(self) -> None:
        self.thread.start()

    def _run(self) -> None:
        while not self.stopping.is_set():
            self.process = subprocess.Popen(
                ["/usr/bin/ssh", "-o", "BatchMode=yes", "-o", "ExitOnForwardFailure=yes",
                 "-o", "ServerAliveInterval=20", "-o", "ServerAliveCountMax=3",
                 "-N", "-L", f"127.0.0.1:{self.local_port}:127.0.0.1:{self.remote_port}",
                 self.target], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.process.wait()
            self.process = None
            self.stopping.wait(10)

    def stop(self) -> None:
        self.stopping.set()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
        self.thread.join(timeout=3)
