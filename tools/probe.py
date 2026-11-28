#!/usr/bin/env python3
"""Collect game data for RS64AP with the rs64ap-probe mod, and turn its log into a report or into data/rs64ap.json.

    python tools/probe.py collect --game "C:/Games/RS64/RogueSquadron64Recomp.exe"
        Drives the game through all 19 missions by itself (enter, wait a few seconds, abort), using the recomp's
        own boot-target navigation, then prints the report. Takes a few minutes; don't touch the game window.

    python tools/probe.py parse [--log <probe.log> | --game <exe>] [--write]
        Reads a probe log (also one from normal play), prints a per-mission report. --write puts the objective
        counts into data/rs64ap.json and regenerates the apworld and client data.

The probe mod must be installed in the game's mods/ folder and enabled in F6. Its log is
mod_data/rs64ap-probe/probe.log beside the game's settings (the game's folder). Turn the RS64AP client mod off while
collecting. Back up your save first: the navigation uses or creates a pilot to fly with.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data" / "rs64ap.json"
LINE = re.compile(r"probe (\w+)((?: \w+=\S*)*)")

# Bonus missions only appear in SELECT LEVEL once unlocked; the nav types their passcodes first.
BONUS_PASSCODES = {16: "WOMPRAT!", 17: "WOISTHAN", 18: "DEADDACK"}


# Used when the script runs on its own (from the probe zip) without the repository's data file.
FALLBACK_NAMES = [
    "Ambush at Mos Eisley", "Rendezvous on Barkhesh", "The Search for the Nonnah", "Defection at Corellia",
    "Liberation of Gerrard V", "The Jade Moon", "Imperial Construction Yards", "Assault on Kile II",
    "Rescue on Kessel", "Prisons of Kessel", "Battle Above Taloraan", "Escape from Fest", "Blockade on Chandrila",
    "Raid on Sullust", "Moff Seerdon's Revenge", "The Battle of Calamari", "Beggar's Canyon",
    "The Death Star Trench Run", "The Battle of Hoth",
]


def mission_names() -> list[str]:
    if DATA.exists():
        return [m["name"] for m in json.loads(DATA.read_text(encoding="utf-8"))["missions"]]
    return FALLBACK_NAMES


def log_path(game: Path) -> Path:
    return game.parent / "mod_data" / "rs64ap-probe" / "probe.log"


# ---- collect ---------------------------------------------------------------------------------------------------

def run_game(game: Path, env_extra: dict[str, str], timeout: int) -> None:
    env = dict(os.environ, **env_extra)
    shown = " ".join(f"{k}={v}" for k, v in env_extra.items())
    print(f"  running: {shown}")
    proc = subprocess.Popen([str(game)], cwd=str(game.parent), env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f"  still running after {timeout} s: stopping it")
        proc.kill()
        proc.wait()


def collect(args: argparse.Namespace) -> int:
    game = Path(args.game).resolve()
    if not game.is_file():
        print(f"no game executable at {game}")
        return 2
    log = log_path(game)
    start = log.stat().st_size if log.exists() else 0
    hold = str(args.hold)

    print("Campaign missions 1-16 (one launch, abort after each):")
    run_game(game, {"ROGUESQ_BOOT_TARGET": "abort:0", "ROGUESQ_NAV_ABORT_LOOP": "16",
                    "ROGUESQ_NAV_ABORT_HOLD": hold}, args.timeout)
    if not args.skip_bonus:
        for level, code in BONUS_PASSCODES.items():
            print(f"Bonus mission {level}:")
            run_game(game, {"ROGUESQ_BOOT_TARGET": f"abort:{level}", "ROGUESQ_NAV_ABORT_LOOP": "1",
                            "ROGUESQ_NAV_ABORT_HOLD": hold, "ROGUESQ_CHEATS": code}, args.timeout // 4)

    if not log.exists() or log.stat().st_size <= start:
        print(f"\nNothing new in {log}. Is the rs64ap-probe mod installed and enabled in F6?")
        return 1
    with log.open(encoding="utf-8", errors="replace") as f:
        f.seek(start)
        lines = f.read().splitlines()
    return report(lines, args.write)


# ---- parse -----------------------------------------------------------------------------------------------------

def parse_events(lines: list[str]) -> list[tuple[str, dict[str, str]]]:
    events = []
    for line in lines:
        m = LINE.search(line)
        if m:
            events.append((m.group(1), dict(kv.split("=", 1) for kv in m.group(2).split())))
    return events


def report(lines: list[str], write: bool) -> int:
    names = mission_names()
    events = parse_events(lines)
    counts: dict[int, set[int]] = defaultdict(set)
    order: dict[int, list[list[int]]] = defaultdict(list)     # per run: objective bits in completion order
    hidden: dict[int, set[str]] = defaultdict(set)
    results: dict[int, list[str]] = defaultdict(list)
    hangar: dict[int, set[str]] = defaultdict(set)
    medals: list[str] = []
    pickups: dict[int, set[str]] = defaultdict(set)
    current: dict[int, list[int]] = {}

    for kind, kv in events:
        lv = int(kv.get("level", "-1"))
        if kind == "mission_ready":
            counts[lv].add(int(kv["objectives"]))
            current[lv] = []
            if kv["hidden"] != "0x00":
                hidden[lv].add(f"start {kv['hidden']}")
        elif kind == "objective":
            counts[lv].add(int(kv["objectives"]))
            new = int(kv["new_done"], 16)
            current.setdefault(lv, []).extend(b for b in range(8) if new >> b & 1)
            if kv["new_hidden"] != "0x00":
                hidden[lv].add(f"{kv['new_hidden']} at {kv['t']}s")
        elif kind == "mission_end":
            counts[lv].add(int(kv["objectives"]))
            done = int(kv["done"], 16)
            run = current.setdefault(lv, [])
            run.extend(b for b in range(8) if done >> b & 1 and b not in run)
            results[lv].append(kv["result"])
            if current.get(lv):
                order[lv].append(current.pop(lv))
        elif kind == "hangar":
            hangar[lv].add(kv["allowed"])
        elif kind == "medal":
            medals.append(f"{names[lv] if 0 <= lv < len(names) else lv}: saved={kv['saved']} live={kv['live']} "
                          f"({kv['where']})")
        elif kind == "upgrade_gained":
            pickups[lv].add(kv["bits"])

    print("\nMission                         Objectives  Completion order (bits)   Hidden flags      Results")
    found: dict[int, int] = {}
    for lv, name in enumerate(names):
        c = sorted(counts.get(lv, set()))
        found[lv] = c[-1] if c else None  # the most a run saw: objectives can be added mid-mission
        orders = "; ".join(",".join(map(str, o)) for o in order.get(lv, [])) or "-"
        print(f"{name:31} {('/'.join(map(str, c)) or '-'):11} {orders:25} "
              f"{(', '.join(sorted(hidden.get(lv, set()))) or '-'):17} {','.join(results.get(lv, [])) or '-'}")

    if pickups:
        print("\nUpgrade bits picked up: " + ", ".join(f"{names[lv]} {'/'.join(sorted(b))}" for lv, b in sorted(pickups.items())))
    if hangar:
        print("Hangar masks: " + ", ".join(f"{names[lv]} {'/'.join(sorted(m))}" for lv, m in sorted(hangar.items())
                                           if 0 <= lv < len(names)))
    if medals:
        print("Medal writes:\n  " + "\n  ".join(medals))

    missing = [names[lv] for lv in range(len(names)) if found.get(lv) is None]
    if missing:
        print(f"\nNo objective count yet for: {', '.join(missing)}")
    if write:
        write_counts({lv: n for lv, n in found.items() if n is not None}, complete=not missing)
    return 0


def write_counts(found: dict[int, int], complete: bool) -> None:
    if not DATA.exists():
        print("\n--write needs the RS64AP repository (data/rs64ap.json); send the report or probe.log instead.")
        return
    d = json.loads(DATA.read_text(encoding="utf-8"))
    for m in d["missions"]:
        if m["id"] in found:
            m["objectives"] = found[m["id"]]
    # Counts alone don't make the option usable: which flag bit each objective sets still has to be confirmed
    # (verified.objectives, set by hand once play logs show the bits).
    d["verified"]["objective_counts"] = complete
    DATA.write_text(json.dumps(d, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"\nWrote {len(found)} objective count(s) to {DATA.relative_to(ROOT)}")
    subprocess.run([sys.executable, str(ROOT / "tools" / "gen_data.py")], check=True)


def parse(args: argparse.Namespace) -> int:
    log = Path(args.log) if args.log else log_path(Path(args.game).resolve()) if args.game else None
    if not log or not log.exists():
        print("give --log <probe.log> or --game <exe>")
        return 2
    return report(log.read_text(encoding="utf-8", errors="replace").splitlines(), args.write)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("collect", help="drive the game through every mission and report")
    c.add_argument("--game", required=True, help="path to RogueSquadron64Recomp(.exe)")
    c.add_argument("--hold", type=int, default=6, help="seconds to stay in each mission before aborting")
    c.add_argument("--timeout", type=int, default=1200, help="seconds before a launch is stopped")
    c.add_argument("--skip-bonus", action="store_true", help="campaign missions only")
    c.add_argument("--write", action="store_true", help="write the counts into data/rs64ap.json")
    p = sub.add_parser("parse", help="report from an existing probe log")
    p.add_argument("--log", help="path to probe.log")
    p.add_argument("--game", help="path to the game executable (finds its probe.log)")
    p.add_argument("--write", action="store_true", help="write the counts into data/rs64ap.json")
    args = ap.parse_args()
    return collect(args) if args.cmd == "collect" else parse(args)


if __name__ == "__main__":
    sys.exit(main())
