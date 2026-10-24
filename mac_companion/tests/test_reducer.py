import unittest
from bridge.reducer import Reducer, public_session_id
from bridge.account_probe import normalize_rate, normalize_usage


class TestReducer(unittest.TestCase):
    def setUp(self):
        self.r = Reducer("testmac")

    def e(self, name, at=100.0, session="private-thread-id"):
        return self.r.ingest({"hook_event_name": name, "session_id": session, "turn_id": "turn-secret"}, at=at)

    def test_lifecycle(self):
        self.e("SessionStart")
        self.assertEqual(self.r.snapshot(101)["focused_phase"], "idle")
        ev = self.e("UserPromptSubmit", 102)
        self.assertEqual(ev["kind"], "codex.turn.started")
        self.assertEqual(self.r.snapshot(103)["running_count"], 1)
        self.e("PreToolUse", 104)
        ev = self.e("PermissionRequest", 105)
        self.assertEqual(ev["kind"], "codex.approval.requested")
        self.assertEqual(self.r.snapshot(106)["approval_count"], 1)
        self.e("PostToolUse", 107)
        self.assertEqual(self.r.snapshot(108)["focused_phase"], "running")
        ev = self.e("Stop", 109)
        self.assertFalse(ev["outcome_verified"])
        self.assertEqual(self.r.snapshot(110)["focused_phase"], "turn_stopped_unverified")

    def test_no_sensitive_text(self):
        ev = self.r.ingest({"hook_event_name": "UserPromptSubmit", "session_id": "sensitive-id", "turn_id": "secret", "prompt": "PASSWORD"}, 100)
        self.assertNotIn("PASSWORD", str(ev))
        self.assertNotIn("sensitive-id", str(ev))

    def test_stale_is_not_success(self):
        self.e("UserPromptSubmit", 1)
        self.assertEqual(self.r.snapshot(610)["focused_phase"], "stale_unknown")

    def test_account_limits_are_not_exact_tokens(self):
        sample = {"rateLimitsByLimitId": {"codex": {
            "primary": {"usedPercent": 65, "windowDurationMins": 300, "resetsAt": 12345},
            "secondary": None}}}
        x = normalize_rate(sample)
        self.assertEqual(x[0]["windows"][0]["remaining_percent"], 35)
        self.assertNotIn("remaining_tokens", str(x))

    def test_account_missing(self):
        self.assertEqual(normalize_rate({}), [])
        self.assertIsNone(normalize_usage(None))

    def test_multi_threads(self):
        self.e("UserPromptSubmit", 10, "thread1")
        self.e("PermissionRequest", 12, "thread2")
        snap = self.r.snapshot(13)
        self.assertEqual(snap["approval_count"], 1)
        self.assertEqual(snap["focused_session_ref"], public_session_id("thread2"))


if __name__ == "__main__":
    unittest.main()
