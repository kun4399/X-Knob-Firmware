import json
import plistlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from bridge import app_config
from bridge.install_hooks import build_app_config, clean_project_hooks, owned_command, EVENTS


class TestAppConfig(unittest.TestCase):
    def test_old_layout_and_current_hooks_are_replaced_without_touching_others(self):
        legacy={"type":"command","command":"/old/python /tmp/X-Knob/1.Firmware/mac_companion/bridge/hook_sender.py"}
        other={"type":"command","command":"/usr/bin/python3 /tmp/other/hook_sender.py"}
        config={"otherSetting":42,"hooks":{"PreToolUse":[{"matcher":"*","hooks":[legacy,other]}],"Stop":[{"hooks":[legacy]}]}}
        cleaned=clean_project_hooks(config,stale_only=True)
        self.assertEqual(cleaned["hooks"]["PreToolUse"],[{"matcher":"*","hooks":[other]}])
        self.assertNotIn("Stop",cleaned["hooks"])
        self.assertEqual(config["hooks"]["PreToolUse"][0]["hooks"],[legacy,other])
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/"X-Knob Companion.app/Contents/MacOS/XKnobCompanion"
            exe.parent.mkdir(parents=True)
            (exe.parent.parent/"Info.plist").write_bytes(plistlib.dumps({"CFBundleIdentifier":"local.xknob.companion.app"}))
            exe.touch()
            new=build_app_config(cleaned,exe)
            self.assertEqual(set(new["hooks"]),set(EVENTS)|{"PreToolUse"})
            self.assertEqual(new,build_app_config(new,exe))
            self.assertEqual(clean_project_hooks(new),cleaned)

    def test_migration_reads_env_as_data_and_preserves_ssh_connection(self):
        with tempfile.TemporaryDirectory() as temp:
            env=Path(temp)/"old.env"
            env.write_text('XKNOB_MQTT_HOST=127.0.0.1\nXKNOB_MQTT_PORT=18830\nXKNOB_SSH_TUNNEL_TARGET=example-nas\nXKNOB_MAC_ID=example-host\nXKNOB_MQTT_USER=example-user\nXKNOB_MQTT_PASSWORD=do-not-copy\n')
            result=app_config.load_legacy(env)
            self.assertEqual((result["port"],result["localPort"],result["sshTarget"]),(1883,18830,"example-nas"))
            self.assertNotIn("do-not-copy",json.dumps(result))

    def test_invalid_ports_and_ssh_options_are_rejected(self):
        for patch_value in ({"port":0},{"port":True},{"hostID":"bad/topic"},{"sshTarget":"-oProxyCommand=bad"}):
            with self.assertRaises(ValueError): app_config.validate({"host":"localhost",**patch_value})

    def test_deleted_app_hooks_remain_removable_without_matching_other_apps(self):
        self.assertTrue(owned_command("'/missing/X-Knob Companion.app/Contents/MacOS/XKnobCompanion' --hook"))
        self.assertFalse(owned_command("'/missing/Other.app/Contents/MacOS/XKnobCompanion' --hook"))

    def test_atomic_updates_back_up_each_revision_with_private_permissions(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(app_config,"BASE",Path(temp)):
            path=Path(temp)/"hooks.json"
            app_config.atomic_json(path,{"revision":1})
            app_config.atomic_json(path,{"revision":2},backup=True)
            app_config.atomic_json(path,{"revision":3},backup=True)
            archives=list((Path(temp)/"backups").iterdir())
            self.assertEqual(len(archives),2)
            self.assertEqual(sorted(json.loads(p.read_text())["revision"] for p in archives),[1,2])
            self.assertEqual(path.stat().st_mode & 0o777,0o600)
            self.assertTrue(all(p.stat().st_mode & 0o777 == 0o600 for p in archives))
