#!/usr/bin/env python3
"""Package release artifacts into dist/.

    python tools/package.py                         rs64.apworld + RS64AP.yaml
    python tools/package.py --client <build dir>    also rs64ap-client-<platform>.zip from a client build
"""
from __future__ import annotations

import argparse
import json
import platform
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / "dist"


def package_apworld() -> Path:
    src = ROOT / "apworld" / "rs64"
    out = DIST / "rs64.apworld"
    manifest = json.loads((src / "archipelago.json").read_text())
    # APContainer fields a packaged .apworld needs (worlds/Files.py, container version 7 since AP 0.6.x).
    manifest.update({"version": 7, "compatible_version": 7})
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(src.rglob("*")):
            if f.is_file() and "__pycache__" not in f.parts and f.suffix != ".pyc" and f.name != "archipelago.json":
                z.write(f, Path("rs64") / f.relative_to(src))
        z.writestr("rs64/archipelago.json", json.dumps(manifest, indent=2) + "\n")
    return out


def package_client(build: Path, platform_name: str | None) -> Path:
    folder = build / "rs64ap"
    if not (folder / "mod.json").exists():
        raise SystemExit(f"no mod folder at {folder}")
    name = platform_name or {"Windows": "windows", "Linux": "linux", "Darwin": "macos"}.get(
        platform.system(), platform.system().lower())
    out = DIST / f"rs64ap-client-{name}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(folder.iterdir()):
            if f.is_file() and f.suffix not in (".lib", ".exp", ".pdb", ".ilk"):
                z.write(f, Path("rs64ap") / f.name)
    return out


def package_probe(build: Path, platform_name: str | None) -> Path:
    folder = build / "rs64ap-probe"
    if not (folder / "mod.json").exists():
        raise SystemExit(f"no probe mod folder at {folder}")
    name = platform_name or {"Windows": "windows", "Linux": "linux", "Darwin": "macos"}.get(
        platform.system(), platform.system().lower())
    out = DIST / f"rs64ap-probe-{name}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(folder.iterdir()):
            if f.is_file() and f.suffix not in (".lib", ".exp", ".pdb", ".ilk"):
                z.write(f, Path("rs64ap-probe") / f.name)
        z.write(ROOT / "tools" / "probe.py", "probe.py")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--client", type=Path, help="client build directory to zip")
    ap.add_argument("--probe", type=Path, help="probe mod build directory to zip (with tools/probe.py)")
    ap.add_argument("--platform", help="platform name in the zip's file name (default: this machine's)")
    args = ap.parse_args()

    if subprocess.run([sys.executable, str(ROOT / "tools" / "gen_data.py"), "--check"]).returncode:
        return 1
    DIST.mkdir(exist_ok=True)
    print(package_apworld())
    shutil.copy(ROOT / "RS64AP.yaml", DIST / "RS64AP.yaml")
    print(DIST / "RS64AP.yaml")
    if args.client:
        print(package_client(args.client, args.platform))
    if args.probe:
        print(package_probe(args.probe, args.platform))
    return 0


if __name__ == "__main__":
    sys.exit(main())
