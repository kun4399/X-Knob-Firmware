import json
import unittest
from pathlib import Path
from bridge.install_hooks import build_config, EVENTS


class TestHooks(unittest.TestCase):
    def test_merge_preserves_existing(self):
        config = {"description": "existing", "hooks": {"Stop": [{"hooks": [{"type": "command", "command": "other"}]}]}}
        data = build_config(config, Path("/tmp/hook_sender.py"), "/usr/bin/python3")
        self.assertEqual(data["description"], "existing")
        self.assertEqual(len(data["hooks"]["Stop"]), 2)
        self.assertEqual(set(data["hooks"]), set(EVENTS))
        data2 = build_config(data, Path("/tmp/hook_sender.py"), "/usr/bin/python3")
        self.assertEqual(len(data2["hooks"]["Stop"]), 2)

if __name__ == "__main__":
    unittest.main()
