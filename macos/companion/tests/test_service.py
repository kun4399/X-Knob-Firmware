import json
import threading
import unittest
from unittest.mock import patch
from bridge.reducer import Reducer
from bridge.service import Service


class FakePublisher:
    def __init__(self):
        self.messages = []

    def publish(self, kind, data, retain):
        self.messages.append((kind, data, retain))


class TestService(unittest.TestCase):
    def setUp(self):
        # Exercise the real processing path without Keychain, hooks, sockets or MQTT.
        self.service = Service.__new__(Service)
        self.service.reducer = Reducer("example-host")
        self.service.publisher = FakePublisher()
        self.service.lock = threading.Lock()
        self.service.last_status_signature = None
        self.service.events_seen = 0

    def send(self, kind, sid="example-session"):
        self.service.process({"session_id": sid, "hook_event_name": kind})

    def test_tool_burst_has_one_semantic_status_and_no_light_commands(self):
        self.send("UserPromptSubmit")
        for _ in range(30):
            self.send("PreToolUse")
            self.send("PostToolUse")
        messages = self.service.publisher.messages
        self.assertEqual(sum(kind == "status" for kind, _, _ in messages), 1)
        self.assertEqual(sum(kind == "event" for kind, _, _ in messages), 1)
        self.assertTrue(all(kind in {"status", "event"} for kind, _, _ in messages))

    def test_approval_status_precedes_event_and_keeps_replay_watermark(self):
        self.send("PermissionRequest")
        self.send("PermissionRequest")
        messages = self.service.publisher.messages
        self.assertEqual([kind for kind, _, _ in messages], ["status", "event"] * 2)
        for index in (0, 2):
            status, event = messages[index][1], messages[index+1][1]
            self.assertEqual(status["boot_id"], event["boot_id"])
            self.assertEqual(status["event_seq"], event["seq"])
            self.assertTrue(messages[index][2])
            self.assertFalse(messages[index+1][2])

    def test_compact_status_keeps_all_session_counts(self):
        for index in range(8):
            self.send("PermissionRequest", f"synthetic-{index}")
        status = self.service.status_snapshot()
        self.assertEqual(len(status["sessions"]), 4)
        self.assertEqual(status["approval_count"], 8)
        self.assertLessEqual(len(json.dumps(status, separators=(",", ":")).encode()), 768)

    def test_unchanged_status_is_kept_alive_every_120_seconds(self):
        with patch("bridge.service.time.sleep", side_effect=[None, RuntimeError("end test")]) as sleep:
            with self.assertRaisesRegex(RuntimeError, "end test"):
                self.service.status_loop()
        self.assertEqual(sleep.call_args_list[0].args, (120,))
        self.assertEqual(self.service.publisher.messages[0][0], "status")
        self.assertTrue(self.service.publisher.messages[0][2])


if __name__ == "__main__":
    unittest.main()
