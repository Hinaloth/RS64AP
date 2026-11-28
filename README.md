# RS64AP
Archipelago Randomizer Implementation for RogueSquadron64Recomp

An [Archipelago](https://archipelago.gg) multiworld randomizer for *Star Wars: Rogue Squadron* (N64), played on
[RogueSquadron64Recomp](https://github.com/MikeSemicolonD/RogueSquadron64Recomp).

- **Items:** 19 Mission Passes (campaign and bonus missions), 9 ships (Naboo N-1 and T-16 included), 8 upgrades,
  Replenishing Secondaries, a Medal Leniency per mission, and filler (Extra Life, Secondary Refill, Hull Repair,
  Temporary Invincibility).
- **Checks:** mission completions, bronze / silver / gold medals on every mission, and the hidden upgrade pickups.
  Objective checks are **work in progress** (see below).
- **Goal:** finish The Battle of Calamari, or finish a set number of missions.

**Download:** the [latest release](https://github.com/Hinaloth/RS64AP/releases/latest) has `rs64.apworld`, the
`RS64AP.yaml` template, and the client mod for Windows and Linux. The full source is in this repository (see
[Repository layout](#repository-layout)); the design and build notes are in [docs/BIBLE.md](docs/BIBLE.md).

## Status: alpha, playable

A full seed has been played from start to goal on Windows against archipelago.gg.

| Works in play | Notes |
| --- | --- |
| Connecting, pilot binding, reconnecting | A pilot is bound to one seed; it reconnects by itself while the server stays the same |
| Mission Passes and mission select | Missions without a pass are listed but can't be confirmed |
| Ships and the hangar | The hangar offers only owned ships the seed's logic counts for that mission (N-1 included) |
| Completions, medals, upgrade pickups | Medal checks go out a few seconds after the debrief; a medal also sends every tier below it |
| Upgrades, Replenishing Secondaries, filler, Medal Leniency | Filler waits for an active mission |

| Not yet | |
| --- | --- |
| Objective checks (**WIP**) | The `objective_checks` option exists but isn't usable yet: it needs each mission's objective count and how objectives map to the game's flags (being collected with the probe, see below). Generation stops with an error if it's turned on, so leave it `false` |
| The Battle of Hoth's ships | Assumed Snowspeeder only |
| DeathLink | Built and tested without the game only |
| Linux client | Built on Ubuntu 24.04 and tested without the game; not yet run with the Linux recomp |

**Ship Logic** (`ship_logic`): `default_only` (the default) means a mission needs its default ship; `any_listed` also
counts the ships vanilla allows on a replay. Both work in play.

## Installing and playing

1. Put `rs64.apworld` in Archipelago's `custom_worlds` folder (or double-click it). Generate a seed from a YAML based on
   `RS64AP.yaml`.
2. Unzip the client for your system into the game's `mods/` folder, so you have `mods/rs64ap/mod.json`.
3. Start the game, press **F6**, enable **Archipelago (RS64AP)** and restart the game.
4. Press **F6** again, click **Archipelago (RS64AP)** to expand it, and fill in **Server** (e.g. `archipelago.gg:38281`),
   **Slot name** and **Password** (if any). These text fields are only editable in the F6 window, not in the in-game
   MODS settings page.
5. Main menu → **ARCHIPELAGO** → pick a **fresh pilot** → **CONNECT**. The pilot is bound to that seed and the client
   rewrites its unlocks, so don't use a pilot you care about. Next time, the same pilot reconnects by itself if the
   server hasn't changed; with a new server address the page waits for CONNECT (and **FORGET PILOT SEED** unbinds it).

The player guide is [apworld/rs64/docs/setup_en.md](apworld/rs64/docs/setup_en.md).

## Collecting game data (probe)

Some game facts the randomizer needs (objective counts per mission, how objectives map to the game's flags) come from
the **rs64ap-probe** mod and `probe.py`, kept separate from the AP client. The probe only writes a log
(`mod_data/rs64ap-probe/probe.log` in the game folder) and changes nothing in the game.

1. Unzip `rs64ap-probe-<os>.zip` into the game's `mods/` folder (it holds the `rs64ap-probe` mod folder and `probe.py`).
   Enable **RS64AP Probe** in F6 and turn the **Archipelago (RS64AP)** mod off. Back up your save.
2. With Python 3.10+: `python probe.py collect --game "<path>/RogueSquadron64Recomp.exe"`. The game starts and
   flies itself into each of the 19 missions, waits a few seconds, aborts, and quits. Don't touch it. The script
   then prints one line per mission: objective count, completion order, hidden flags, results.
3. For the order objectives complete in, play missions through normally with the probe on, then run
   `python probe.py parse --game "<path>/RogueSquadron64Recomp.exe"`.

From the repository, `python tools/probe.py parse --log <probe.log> --write` puts the counts into
`data/rs64ap.json` and regenerates the apworld and client data.

## Testing DeathLink alone

`tools/deathlink_tester.py` (Python 3.10+, `pip install websockets`) joins your slot as a second, chat-only
connection with the DeathLink tag: press Enter to kill your ship, and deaths your game sends are printed. The seed
needs `death_link: true`.

```sh
python tools/deathlink_tester.py --server archipelago.gg:38281 --slot <your slot> [--password ...]
```

## Repository layout

```
apworld/rs64/        the Archipelago world (Python): items, locations, options, logic, tests, player docs
client/              the in-game client, a native mod for the recomp (C++)
probes/rs64ap-probe/ log-only data-collection mod (C++), not part of the client
  src/               connection (net), per-pilot state (pilot), game hooks and rules (mod)
  src/generated/     generated from data/ — do not edit
  tests/harness.cpp  loads the mod with a fake host API for tests
  cmake/             MinGW-w64 toolchain for cross-building Windows from Linux
data/rs64ap.json     missions, ships, upgrades, items, IDs: the single source of truth
tools/               gen_data.py (codegen), e2e.py (end-to-end test), package.py (release zips),
                     probe.py (drive the game with the probe mod, parse its log),
                     deathlink_tester.py (send/receive DeathLink without a second game)
docs/BIBLE.md        design, game facts, decision log
RS64AP.yaml          player options template
```

After editing `data/rs64ap.json`, run `python tools/gen_data.py`. CI fails when the generated files are stale.

## Building

### apworld

```sh
python tools/package.py        # writes dist/rs64.apworld and dist/RS64AP.yaml
```

### Client mod

Needs CMake 3.20+, a C++17 compiler, OpenSSL 3 (for `wss://`, which archipelago.gg uses) and zlib (compressed
connections; the server warns without it). The other dependencies (apclientpp, websocketpp, asio, nlohmann/json,
and the recomp's `host_api.h`) are fetched at configure time.

**Linux**

```sh
sudo apt install libssl-dev zlib1g-dev
cmake -S client -B build/client -DCMAKE_BUILD_TYPE=Release
cmake --build build/client
```

**Windows (MSVC + vcpkg)**

```powershell
vcpkg install openssl:x64-windows-static zlib:x64-windows-static
cmake -S client -B build/client -A x64 -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build/client --config Release
```

**Windows, cross-compiled from Linux (MinGW-w64)**: build OpenSSL 3 and zlib statically for MinGW first, then

```sh
cmake -S client -B build/win -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/client/cmake/mingw-w64-x86_64.cmake -DOPENSSL_ROOT_DIR=<mingw OpenSSL prefix> \
  -DZLIB_ROOT=<mingw zlib prefix> -DZLIB_USE_STATIC_LIBS=ON
cmake --build build/win
```

The release's Windows DLL is built this way: it links its runtime, OpenSSL and zlib statically and needs only
Windows system DLLs.

The mod folder is `build/client/rs64ap/`. `python tools/package.py --client build/client` zips it into `dist/`.

Options: `-DRS64_RECOMP_DIR=<recomp checkout>` uses a local `host_api.h` instead of downloading it.
`-DRS64AP_REQUIRE_SSL=ON` fails the build when OpenSSL is missing; without OpenSSL the client only reaches `ws://`
servers.

## Testing

```sh
# apworld: from an Archipelago source checkout with requirements installed
ln -s "$PWD/apworld/rs64" <Archipelago>/worlds/rs64
cd <Archipelago> && python -m pytest worlds/rs64/test

# client, without the game: build with the harness, then
cmake -S client -B build/client -DRS64AP_BUILD_HARNESS=ON && cmake --build build/client
python tools/e2e.py --ap <Archipelago> --build build/client
```

In the game, the recomp's log (stderr) has the client's `[rs64ap]` lines: what it sends and receives. Game facts
still being mapped come from the probe mod instead (see above).

## License

- `client/` and `probes/` are **GPL-3.0-or-later**. They're plugins loaded into RogueSquadron64Recomp (GPL-3.0) and
  built against its header. See [client/NOTICE.md](client/NOTICE.md) for bundled third-party licenses.
- Everything else (apworld, data, tools, docs) is **MIT** ([LICENSE](LICENSE)), the usual license for Archipelago
  worlds.

You need your own legally obtained copy of the game. This project isn't affiliated with Lucasfilm, Nintendo,
Factor 5 or the recomp's authors.
