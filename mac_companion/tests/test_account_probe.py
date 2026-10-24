import os
import stat
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from bridge.account_probe import query_account

FAKE_CODEX = '''#!/usr/bin/env python3
import sys, json
for line in sys.stdin:
    msg=json.loads(line)
    if msg.get("id")==1:
        print(json.dumps({"id":1,"result":{"userAgent":"fake"}}), flush=True)
    if msg.get("id")==2:
        print(json.dumps({"id":2,"result":{"rateLimitsByLimitId":{"codex":{"primary":{"usedPercent":30,"resetsAt":1870000000,"windowDurationMins":300}}}}}), flush=True)
    if msg.get("id")==3:
        print(json.dumps({"id":3,"result":{"summary":{"lifetimeTokens":3456},"dailyUsageBuckets":[{"startDate":"2026-10-24","tokens":234}]}}), flush=True)
'''

class TestAccountProbe(unittest.TestCase):
    def test_handshake_and_normalization(self):
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'fake_codex'
            f.write_text(FAKE_CODEX)
            f.chmod(f.stat().st_mode | stat.S_IXUSR)
            with patch.dict(os.environ, {'XKNOB_CODEX_BIN':str(f)}):
                x=query_account(timeout_s=3)
            self.assertEqual(x['quality'], 'observed')
            self.assertEqual(x['rate_limits'][0]['windows'][0]['remaining_percent'],70)
            self.assertEqual(x['token_activity']['lifetime_tokens'],3456)
            self.assertNotIn('accessToken', str(x))

if __name__=='__main__':
    unittest.main()
