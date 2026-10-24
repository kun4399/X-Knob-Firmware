import json
import unittest
from pathlib import Path
from bridge.install_hooks import build_config, remove_config, EVENTS, TOOL_EVENTS


class TestHooks(unittest.TestCase):
    def test_merge_preserves_existing(self):
        config = {"description": "existing", "hooks": {"Stop": [{"hooks": [{"type": "command", "command": "other"}]}]}}
        data = build_config(config, Path("/tmp/hook_sender.py"), "/usr/bin/python3")
        self.assertEqual(data["description"], "existing")
        self.assertEqual(len(data["hooks"]["Stop"]), 2)
        self.assertEqual(set(data["hooks"]), set(EVENTS))
        data2 = build_config(data, Path("/tmp/hook_sender.py"), "/usr/bin/python3")
        self.assertEqual(len(data2["hooks"]["Stop"]), 2)

    def test_legacy_high_frequency_hooks_are_removed_on_upgrade(self):
        script = Path("/tmp/hook_sender.py")
        py = "/usr/bin/python3"
        old = build_config({}, script, py, include_tools=True)
        self.assertIn("PreToolUse", old["hooks"])
        new = build_config(old, script, py)
        self.assertNotIn("PreToolUse", new["hooks"])
        self.assertNotIn("PostToolUse", new["hooks"])
        self.assertEqual(set(new["hooks"]), set(EVENTS))
        verbose = build_config(new, script, py, include_tools=True)
        self.assertEqual(set(verbose["hooks"]), set(EVENTS + TOOL_EVENTS))

    def test_upgrade_preserves_mixed_group_and_changed_interpreter(self):
        script = Path("/tmp/example/macos/companion/bridge/hook_sender.py")
        old_sender = "/tmp/example/macos/bridge/hook_sender.py"
        ours = {"type": "command", "command": f"/old/venv/python {old_sender}", "timeout": 3}
        other = {"type": "command", "command": "echo custom"}
        config = {"hooks": {"PreToolUse": [{"matcher": "*", "hooks": [ours, other]}]}}
        new = build_config(config, script, "/new/venv/python")
        self.assertEqual(new["hooks"]["PreToolUse"], [{"matcher": "*", "hooks": [other]}])
        self.assertEqual(len(config["hooks"]["PreToolUse"][0]["hooks"]), 2)
        self.assertEqual(remove_config(new, script, "/different/python")["hooks"],
                         {"PreToolUse": [{"matcher": "*", "hooks": [other]}]})

    def test_unrelated_sender_is_preserved(self):
        config = {"hooks": {"Stop": [{"hooks": [{"type": "command", "command":
            "/usr/bin/python3 /tmp/another-project/hook_sender.py"}]}]}}
        result = remove_config(config, Path("/tmp/our-project/hook_sender.py"), "python3")
        self.assertEqual(result, config)

if __name__ == "__main__":
    unittest.main()
