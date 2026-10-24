"""Packaged backend entrypoint. Settings contain no password; stdin carries it."""
import argparse
import json
import os
import signal
import threading
import time
from . import app_config


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("action",choices=["migrate","install-hooks","uninstall-hooks","hook-status","serve"])
    parser.add_argument("--executable")
    parser.add_argument("--parent-pid",type=int)
    args=parser.parse_args()
    if args.action=="migrate":
        result=app_config.migrate()
        result["retired_legacy"]=app_config.retire_legacy()
        print(json.dumps(result));return
    if args.action=="install-hooks":
        if not args.executable: raise ValueError("缺少应用路径")
        print(json.dumps(app_config.change_hooks(args.executable)));return
    if args.action=="uninstall-hooks":
        print(json.dumps(app_config.change_hooks()));return
    if args.action=="hook-status":
        _,count=app_config.hook_state();print(json.dumps({"ok":True,"count":count}));return
    settings=app_config.validate(json.loads(app_config.SETTINGS.read_text()))
    import sys
    data=sys.stdin.buffer.read(65537)
    if len(data)>65536: raise ValueError("凭据输入过长")
    password=json.loads(data).get("password","")
    if not isinstance(password,str): raise ValueError("凭据格式错误")
    if args.parent_pid:
        def watch_parent():
            while True:
                time.sleep(2)
                if os.getppid()!=args.parent_pid:
                    os.kill(os.getpid(),signal.SIGTERM);return
        threading.Thread(target=watch_parent,daemon=True).start()
    from .service import main as serve
    serve(settings,password)


if __name__=="__main__":
    try:
        main()
    except (ValueError,RuntimeError,OSError) as error:
        # Messages never contain configured host, username, key or password.
        print(json.dumps({"ok":False,"error":str(error) if isinstance(error,ValueError) else "操作失败，请检查配置或端口占用"}))
        raise SystemExit(1)
