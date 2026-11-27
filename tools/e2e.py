#!/usr/bin/env python3
"""End-to-end client test without the game.

Generates a seed with the apworld, hosts it with Archipelago's MultiServer on localhost, and runs the client test
harness (client/tests/harness.cpp) against it: connect, bind a pilot, receive start items, check enforcement, fly a
starting mission, send its checks, reconnect.

    python tools/e2e.py --ap <Archipelago source checkout> --build <client build dir>

The Archipelago checkout needs its requirements installed, and apworld/rs64 linked or copied to worlds/rs64.
Build the client with -DRS64AP_BUILD_HARNESS=ON first.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import re
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

YAML = """name: Tester
game: Star Wars Rogue Squadron
Star Wars Rogue Squadron:
  goal: calamari
  starting_missions: 2
  medal_checks: silver
  upgrade_pickups: true
  start_inventory:
    Replenishing Secondaries: 1
    Temporary Invincibility: 1
""" + "".join(f"    {m} Medal Leniency: 1\n" for m in (
    "Ambush at Mos Eisley", "Rendezvous on Barkhesh", "The Search for the Nonnah", "Defection at Corellia",
    "Liberation of Gerrard V", "The Jade Moon", "Imperial Construction Yards", "Assault on Kile II",
    "Rescue on Kessel", "Prisons of Kessel", "Battle Above Taloraan", "Escape from Fest", "Blockade on Chandrila",
    "Raid on Sullust", "Moff Seerdon's Revenge", "The Battle of Calamari", "Beggar's Canyon",
    "The Death Star Trench Run", "The Battle of Hoth"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ap", required=True, type=Path, help="Archipelago source checkout")
    ap.add_argument("--build", required=True, type=Path, help="client build directory")
    ap.add_argument("--port", default=38281, type=int)
    ap.add_argument("--seed", default=4242, type=int)
    args = ap.parse_args()

    lib = next((p for p in (args.build / "rs64ap").glob("rs64ap.*") if p.suffix in (".so", ".dll", ".dylib")), None)
    harness = next((p for p in args.build.rglob("rs64ap_harness*") if p.is_file() and os.access(p, os.X_OK)), None)
    if not lib or not harness:
        print("build the client with -DRS64AP_BUILD_HARNESS=ON first")
        return 2

    env = dict(os.environ, SKIP_REQUIREMENTS_UPDATE="1")
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "players").mkdir()
        (tmp / "players" / "tester.yaml").write_text(YAML)
        subprocess.run([sys.executable, "Generate.py", "--player_files_path", str(tmp / "players"),
                        "--outputpath", str(tmp / "out"), "--seed", str(args.seed)],
                       cwd=args.ap, env=env, stdin=subprocess.DEVNULL, check=True, capture_output=True)
        archive = next((tmp / "out").glob("AP_*.zip"))
        with zipfile.ZipFile(archive) as z:
            spoiler = next(n for n in z.namelist() if n.endswith("_Spoiler.txt"))
            text = z.read(spoiler).decode()
            multidata = next(n for n in z.namelist() if n.endswith(".archipelago"))
            z.extract(multidata, tmp)

        # The first pass in sphere 0 is a starting mission.
        sphere0 = text[text.index("Playthrough:"):].split("}", 1)[0]
        spec = importlib.util.spec_from_file_location("rs64_data", ROOT / "apworld" / "rs64" / "data.py")
        data = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(data)
        level = next(m["id"] for m in data.MISSIONS if f"  {m['name']} Pass" in sphere0)
        print(f"flying starting mission {level}: {data.MISSIONS[level]['name']}")

        server = subprocess.Popen([sys.executable, "MultiServer.py", "--host", "127.0.0.1", "--port", str(args.port),
                                   str(tmp / multidata)], cwd=args.ap, env=env, stdin=subprocess.DEVNULL,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            time.sleep(5)
            (tmp / "data").mkdir()
            result = subprocess.run([str(harness), str(lib), f"ws://127.0.0.1:{args.port}", "Tester", str(level),
                                     str(tmp / "data")], timeout=180)
        finally:
            server.terminate()
            out, _ = server.communicate(timeout=10)
        sent = re.findall(r"Tester sent (.+?) to Tester \((.+?)\)", out)
        for item, location in sent:
            print(f"server: {location} -> {item}")
        if not sent:
            print("server saw no checks")
            return 1
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
