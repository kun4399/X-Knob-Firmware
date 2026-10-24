"""Validated app configuration and reversible migration; no credential logging."""
from __future__ import annotations
from pathlib import Path
import datetime
import json
import os
import re
import shlex
import tempfile
import plistlib
import subprocess
from .install_hooks import clean_project_hooks, build_app_config, owned_command

BASE = Path(os.environ.get("XKNOB_CONFIG_DIR",Path.home()/".config/xknob-companion"))
SETTINGS = BASE/"settings.json"


def validate(config):
    result = dict(config)
    host = str(result.get("host", "")).strip()
    if not host or not re.fullmatch(r"[A-Za-z0-9_.:\[\]-]+", host) or host.startswith("-"):
        raise ValueError("请输入有效的 MQTT 服务器地址")
    for key, default in (("port",1883),("localPort",18830)):
        value = result.get(key,default)
        if isinstance(value,bool) or not isinstance(value,int) or not 1<=value<=65535:
            raise ValueError("端口应在 1–65535 之间")
        result[key] = value
    host_id = str(result.get("hostID", "example-host"))
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,48}",host_id):
        raise ValueError("配对标识只能含字母、数字、短横线和下划线，最长 48 字符")
    ssh = str(result.get("sshTarget", "")).strip()
    if ssh and (ssh.startswith("-") or not re.fullmatch(r"[A-Za-z0-9_.@:\[\]-]+",ssh)):
        raise ValueError("请输入 SSH 配置中的连接名称或 user@host")
    if not isinstance(result.get("tls",False),bool):
        raise ValueError("TLS 设置无效")
    result.update(host=host,hostID=host_id,sshTarget=ssh,
                  username=str(result.get("username","")),tls=result.get("tls",False),
                  accountPoll=bool(result.get("accountPoll",False)))
    # Secrets are accepted only over stdin/Keychain, never configuration JSON.
    return {k:result[k] for k in ("host","port","username","hostID","sshTarget","localPort","tls","accountPoll")}


def atomic_json(path: Path, value, *, backup=False):
    path.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
    before=path.read_bytes() if path.exists() else None
    if backup and before is not None:
        archive=BASE/"backups";archive.mkdir(mode=0o700,parents=True,exist_ok=True)
        fd,name=tempfile.mkstemp(prefix=path.name+"-"+datetime.datetime.now().strftime("%Y%m%d-%H%M%S")+"-",dir=archive)
        with os.fdopen(fd,"wb") as out: out.write(before)
    fd,name=tempfile.mkstemp(prefix="."+path.name+"-",dir=path.parent)
    try:
        with os.fdopen(fd,"w") as out:
            json.dump(value,out,ensure_ascii=False,indent=2);out.write("\n");out.flush();os.fsync(out.fileno())
        if (path.read_bytes() if path.exists() else None) != before:
            raise RuntimeError("配置刚被其他程序修改，请重试")
        os.replace(name,path)
    finally:
        if os.path.exists(name): os.unlink(name)


def load_legacy(path: Path):
    values={}
    if path.exists():
        for line in path.read_text().splitlines():
            line=line.strip().removeprefix("export ")
            if line.startswith("XKNOB_") and "=" in line:
                key,value=line.split("=",1)
                parts=shlex.split(value,comments=True)
                values[key]=parts[0] if parts else ""
    ssh=values.get("XKNOB_SSH_TUNNEL_TARGET","")
    return validate({"host":values.get("XKNOB_MQTT_HOST","127.0.0.1"),
                     "port":1883 if ssh else int(values.get("XKNOB_MQTT_PORT","1883")),
                     "localPort":int(values.get("XKNOB_MQTT_PORT","18830")) if ssh else 18830,
                     "username":values.get("XKNOB_MQTT_USER",""),
                     "hostID":values.get("XKNOB_MAC_ID","example-host"),"sshTarget":ssh,
                     "tls":False,"accountPoll":values.get("XKNOB_ENABLE_ACCOUNT_POLL")=="1"})


def hook_state(path=None):
    path=path or Path(os.environ.get("XKNOB_HOOKS_CONFIG",Path.home()/".codex/hooks.json"))
    existing=json.loads(path.read_text()) if path.exists() else {}
    count=sum(1 for groups in existing.get("hooks",{}).values() if isinstance(groups,list)
              for group in groups if isinstance(group,dict) for h in group.get("hooks",[])
              if isinstance(h,dict) and owned_command(h.get("command","")))
    return existing,count


def change_hooks(executable=None, *, stale_only=False, path=None):
    path=path or Path(os.environ.get("XKNOB_HOOKS_CONFIG",Path.home()/".codex/hooks.json"))
    existing,before=hook_state(path)
    new=build_app_config(existing,Path(executable)) if executable else clean_project_hooks(existing,stale_only=stale_only)
    if new != existing: atomic_json(path,new,backup=True)
    _,after=hook_state(path)
    return {"ok":True,"before":before,"after":after,"changed":new!=existing}


def retire_legacy():
    path=Path.home()/"Library/LaunchAgents/local.xknob.companion.plist"
    if not path.exists(): return False
    data=plistlib.loads(path.read_bytes())
    args=data.get("ProgramArguments",[])
    if data.get("Label")!="local.xknob.companion" or not any(
        "X-Knob" in Path(a).parts and a.endswith("run_companion.sh") for a in args):
        raise RuntimeError("现有同名后台项目不属于 X-Knob，未更改")
    archive=BASE/"backups";archive.mkdir(mode=0o700,parents=True,exist_ok=True)
    fd,name=tempfile.mkstemp(prefix="legacy-launchagent-",suffix=".plist",dir=archive)
    with os.fdopen(fd,"wb") as out: out.write(path.read_bytes())
    command=subprocess.run(["/bin/launchctl","bootout",f"gui/{os.getuid()}",str(path)],capture_output=True)
    if command.returncode not in (0,3,5,36,113):
        raise RuntimeError("旧后台服务无法停止，未启动重复服务")
    path.unlink()
    return True


def migrate():
    BASE.mkdir(mode=0o700,parents=True,exist_ok=True)
    if not SETTINGS.exists(): atomic_json(SETTINGS,load_legacy(BASE/"companion.env"))
    else: validate(json.loads(SETTINGS.read_text()))
    return change_hooks(stale_only=True)
