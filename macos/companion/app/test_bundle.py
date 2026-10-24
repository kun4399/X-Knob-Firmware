#!/usr/bin/env python3
"""Exercise the actual built bundle with isolated config/ports and synthetic events."""
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json,os,socket,subprocess,tempfile,threading,time,urllib.request,urllib.error

APP=Path(__file__).resolve().parents[1]/"dist/X-Knob Companion.app"
NATIVE=APP/"Contents/MacOS/XKnobCompanion"
BACKEND=APP/"Contents/Resources/backend/XKnobBridge"


def main():
    received=[]
    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            received.append(json.loads(self.rfile.read(int(self.headers['Content-Length']))))
            self.send_response(204);self.end_headers()
        def log_message(self,*args): pass
    with tempfile.TemporaryDirectory(prefix="xknob-app-test-") as temp:
        root=Path(temp);hooks=root/"hooks.json"
        hooks.write_text(json.dumps({"hooks":{"Stop":[{"hooks":[{"type":"command","command":"echo keep-me"}]}]}}))
        env={**os.environ,"XKNOB_CONFIG_DIR":str(root),"XKNOB_HOOKS_CONFIG":str(hooks)}
        def command(*args):
            result=subprocess.run([str(BACKEND),*args],env=env,capture_output=True,text=True,timeout=20)
            assert result.returncode==0,result.stderr
            return json.loads(result.stdout)
        installed=command("install-hooks","--executable",str(NATIVE));assert installed['after']==6
        again=command("install-hooks","--executable",str(NATIVE));assert again['after']==6 and not again['changed']
        removed=command("uninstall-hooks");assert removed['after']==0
        assert json.loads(hooks.read_text())['hooks']['Stop'][0]['hooks'][0]['command']=='echo keep-me'
        print('PASS: packaged hook install/reinstall/uninstall preserve unrelated configuration')
        (root/"bridge.key").write_text('synthetic-test-key')
        server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
        threading.Thread(target=server.serve_forever,daemon=True).start()
        payload={"hook_event_name":"Stop","session_id":"synthetic-session","turn_id":"synthetic-turn","prompt":"DO_NOT_FORWARD","command":"PRIVATE_COMMAND","output":"PRIVATE_OUTPUT"}
        forwarding_env={**env,"XKNOB_INGRESS_PORT":str(server.server_port)}
        result=subprocess.run([str(NATIVE),'--hook'],input=json.dumps(payload),env=forwarding_env,capture_output=True,text=True,timeout=5)
        assert result.returncode==0 and json.loads(result.stdout)=={}
        assert received==[{k:payload[k] for k in ('hook_event_name','session_id','turn_id')}],received
        server.shutdown();server.server_close()
        result=subprocess.run([str(NATIVE),'--hook'],input='invalid-json',env=forwarding_env,capture_output=True,text=True,timeout=5)
        assert result.returncode==0
        result=subprocess.run([str(NATIVE),'--hook'],input=json.dumps(payload),env=forwarding_env,capture_output=True,text=True,timeout=5)
        assert result.returncode==0 and json.loads(result.stdout)=={}
        print('PASS: native helper strips private payloads, returns Stop JSON, fails open offline/malformed')

        with socket.socket() as sock: sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        (root/'settings.json').write_text(json.dumps({'host':'127.0.0.1','port':1,'username':'','hostID':'synthetic-host','sshTarget':'','localPort':18830,'tls':False,'accountPoll':False}))
        process=subprocess.Popen([str(BACKEND),'serve','--parent-pid',str(os.getpid())],stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,env={**env,'XKNOB_INGRESS_PORT':str(port)})
        process.stdin.write(b'{"password":"synthetic-password"}');process.stdin.close()
        url=f'http://127.0.0.1:{port}'
        try:
            health=None
            for _ in range(60):
                try: health=json.loads(urllib.request.urlopen(url+'/healthz',timeout=.2).read());break
                except Exception:
                    if process.poll() is not None: raise AssertionError(process.stderr.read().decode())
                    time.sleep(.1)
            assert health and health['pid']==process.pid and not health['mqtt_connected'],health
            try:
                urllib.request.urlopen(urllib.request.Request(url+'/v1/hook',data=b'{}',method='POST'),timeout=1)
                raise AssertionError('Unauthenticated hook accepted')
            except urllib.error.HTTPError as error: assert error.code==403
            token=(root/'bridge.key').read_text().strip()
            request=urllib.request.Request(url+'/v1/hook',data=json.dumps(payload).encode(),headers={'X-Bridge-Key':token},method='POST')
            assert urllib.request.urlopen(request,timeout=1).status==204
            health=json.loads(urllib.request.urlopen(url+'/healthz').read());assert health['events']==1
        finally:
            process.terminate();process.wait(timeout=8)
        assert process.returncode==0
        print('PASS: bundled runtime PID/health, authentication, ingress, graceful shutdown without production traffic')


if __name__=='__main__': main()
