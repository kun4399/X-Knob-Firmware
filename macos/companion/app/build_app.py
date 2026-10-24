#!/usr/bin/env python3
"""Build a self-contained, ad-hoc signed menu-bar app using Swift + bundled backend."""
from pathlib import Path
import os
import plistlib
import shutil
import subprocess
import sys
import re

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build"
DIST=ROOT/"dist"
APP=DIST/"X-Knob Companion.app"


def main():
    BUILD.mkdir(exist_ok=True);DIST.mkdir(exist_ok=True)
    subprocess.run([sys.executable,"-m","PyInstaller","--noconfirm","--clean","--onedir",
        "--name","XKnobBridge","--paths",str(ROOT),"--distpath",str(BUILD/"frozen"),
        "--workpath",str(BUILD/"pyinstaller"),"--specpath",str(BUILD),
        str(ROOT/"app/backend_entry.py")],check=True)
    if APP.exists(): shutil.rmtree(APP)
    contents=APP/"Contents"; (contents/"MacOS").mkdir(parents=True); (contents/"Resources").mkdir()
    shutil.copytree(BUILD/"frozen/XKnobBridge",contents/"Resources/backend",symlinks=True)
    # The bundled Python/OpenSSL can require a newer OS than the native UI.
    # Advertise the real highest deployment target rather than an untested claim.
    minimum=(13,0)
    for path in (contents/"Resources/backend").rglob("*"):
        if not path.is_file() or path.is_symlink(): continue
        with path.open("rb") as file: magic=file.read(4)
        if magic not in (b'\xcf\xfa\xed\xfe',b'\xce\xfa\xed\xfe',b'\xca\xfe\xba\xbe'): continue
        commands=subprocess.check_output(["/usr/bin/otool","-l",str(path)],text=True)
        versions=re.findall(r'\bminos\s+(\d+(?:\.\d+)+)',commands)
        versions+=re.findall(r'cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+)+)',commands)
        for version in versions: minimum=max(minimum,tuple(map(int,version.split('.'))))
    minimum_text='.'.join(map(str,minimum))
    subprocess.run(["/usr/bin/swiftc","-O","-swift-version","5","-target",f"arm64-apple-macos{minimum_text}",
        "-framework","AppKit","-framework","Security","-framework","LocalAuthentication",str(ROOT/"app/XKnobCompanion.swift"),
        "-o",str(contents/"MacOS/XKnobCompanion")],check=True)
    info={"CFBundleIdentifier":"local.xknob.companion.app","CFBundleName":"X-Knob Companion",
        "CFBundleDisplayName":"X-Knob Companion","CFBundleExecutable":"XKnobCompanion",
        "CFBundlePackageType":"APPL","CFBundleShortVersionString":"1.0.0","CFBundleVersion":"1",
        "LSMinimumSystemVersion":minimum_text,"LSUIElement":True,"NSHighResolutionCapable":True,
        "NSAppTransportSecurity":{"NSAllowsLocalNetworking":True}}
    (contents/"Info.plist").write_bytes(plistlib.dumps(info))
    subprocess.run(["/usr/bin/codesign","--force","--deep","--sign","-",str(APP)],check=True)
    subprocess.run(["/usr/bin/codesign","--verify","--deep","--strict",str(APP)],check=True)
    subprocess.run([str(contents/"MacOS/XKnobCompanion"),"--self-test"],check=True)
    print(f"Built and verified: {APP}")
    print(f"Actual bundled minimum macOS: {minimum_text}")


if __name__=="__main__": main()
