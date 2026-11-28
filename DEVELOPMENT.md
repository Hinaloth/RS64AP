# RS64AP development

Everything here is for working on RS64AP itself. Players want [README.md](README.md) and [SETUP.md](SETUP.md).
The full design and build notes, and the decision log of what play confirmed, are in [docs/BIBLE.md](docs/BIBLE.md).

## Repository layout

```
README.md            for players: what the randomizer does, options, locations, items
SETUP.md             for players: installing the game, the mod and the apworld, generating, connecting
apworld/rs64/        the Archipelago world (Python): items, locations, options, logic, tests, player docs
client/              the in-game client, a native mod for the recomp (C++)
  src/               connection (net), per-pilot state (pilot), game hooks and rules (mod)
  src/generated/     generated from data/ — do not edit
  tests/harness.cpp  loads the mod with a fake host API for tests
  cmake/             MinGW-w64 toolchain for cross-building Windows from Linux
probes/rs64ap-probe/ log-only data-collection mod (C++), not part of the client
data/rs64ap.json     missions, ships, upgrades, items, IDs: the single source of truth
tools/               gen_data.py (codegen), e2e.py (end-to-end test), package.py (release zips),
                     probe.py (drive the game with the probe mod, parse its log),
                     deathlink_tester.py (send/receive DeathLink without a second game)
docs/BIBLE.md        design, game facts, decision log
RS64AP.yaml          player options template
```

After editing `data/rs64ap.json`, `SETUP.md` or `README.md`, run `python tools/gen_data.py`: it regenerates the data
files, copies `SETUP.md` to `apworld/rs64/docs/setup_en.md` and the README's gameplay sections to the apworld's game
page. CI fails when any of them is stale.

## Building

### apworld

```sh
python tools/package.py        # writes dist/rs64.apworld and dist/"Star Wars Rogue Squadron.yaml"
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

## Testing DeathLink alone

`tools/deathlink_tester.py` (Python 3.10+, `pip install websockets`) joins your slot as a second, chat-only
connection with the DeathLink tag: press Enter to kill your ship, and deaths your game sends are printed. The seed
needs `death_link: true`.

```sh
python tools/deathlink_tester.py --server archipelago.gg:38281 --slot <your slot> [--password ...]
```

## Collecting game data (probe)

Game facts the randomizer relies on (objective counts per mission, how objectives map to the game's flags, ship
masks) were collected with the **rs64ap-probe** mod and `probe.py`, kept separate from the AP client. They stay
available for checking or extending the data. The probe only writes a log
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

## License

- `client/` and `probes/` are **GPL-3.0-or-later**. They're plugins loaded into RogueSquadron64Recomp (GPL-3.0) and
  built against its header. See [client/NOTICE.md](client/NOTICE.md) for bundled third-party licenses.
- Everything else (apworld, data, tools, docs) is **MIT** ([LICENSE](LICENSE)), the usual license for Archipelago
  worlds.

You need your own legally obtained copy of the game. This project isn't affiliated with Lucasfilm, Nintendo,
Factor 5 or the recomp's authors.
