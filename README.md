# RS64AP
Archipelago Randomizer Implementation for RogueSquadron64Recomp

An [Archipelago](https://archipelago.gg) multiworld randomizer for *Star Wars: Rogue Squadron* (N64), played on
[RogueSquadron64Recomp](https://github.com/MikeSemicolonD/RogueSquadron64Recomp).

- **Items:** 19 Mission Passes (campaign and bonus missions), 9 ships (Naboo N-1 and T-16 included), 8 upgrades, filler.
- **Checks:** mission completions, medals (bronze / silver / gold), hidden upgrade pickups, and later objectives.
- **Goal:** finish The Battle of Calamari, or finish a set number of missions.

The full design and build plan is in [docs/BIBLE.md](docs/BIBLE.md).

## Status: pre-alpha, not yet tested in the game

| Part | State |
| --- | --- |
| apworld | Complete for the current design. Tests pass on Archipelago 0.6.8+, including generation across every option combination. |
| Client mod | Builds on Linux. Tested end to end against a real Archipelago server with a fake game (`tools/e2e.py`): connect, pilot binding, start items, unlock enforcement, mission/ship gating, completion/medal/pickup checks, reconnect. **Not run inside the recomp yet.** |
| Game data | **Provisional.** Which ships each mission allows, the pickups and the objective counts still need checking in the game (spikes S1–S12 in the bible). The client logs `[rs64ap] probe ...` lines with the real values. |

Until spike S1 lands, logic assumes most missions need the X-wing (Gerrard V the V-wing, Beggar's Canyon the T-16,
Hoth the Snowspeeder). The apworld warns about this when it generates.

## Repository layout

```
apworld/rs64/        the Archipelago world (Python)
client/              the in-game client, a native mod for the recomp (C++)
  src/generated/     generated from data/ — do not edit
  tests/harness.cpp  loads the mod with a fake host API for tests
data/rs64ap.json     missions, ships, upgrades, filler, IDs: the single source of truth
tools/               gen_data.py (codegen), e2e.py (end-to-end test), package.py (release zips)
docs/BIBLE.md        design and build plan
RS64AP.yaml          player options template
```

After editing `data/rs64ap.json`, run `python tools/gen_data.py`. CI fails when the generated files are stale.

## Building

### apworld

```sh
python tools/package.py        # writes dist/rs64.apworld and dist/RS64AP.yaml
```

### Client mod

Needs CMake 3.20+, a C++17 compiler and OpenSSL 3 (for `wss://`, which archipelago.gg uses). The other dependencies
(apclientpp, websocketpp, asio, nlohmann/json, and the recomp's `host_api.h`) are fetched at configure time.

**Linux**

```sh
sudo apt install libssl-dev zlib1g-dev
cmake -S client -B build/client -DCMAKE_BUILD_TYPE=Release
cmake --build build/client
```

**Windows (MSVC + vcpkg)**

```powershell
vcpkg install openssl:x64-windows-static
cmake -S client -B build/client -A x64 -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build/client --config Release
```

The mod folder is `build/client/rs64ap/`. `python tools/package.py --client build/client` zips it into `dist/`.

Options: `-DRS64_RECOMP_DIR=<recomp checkout>` uses a local `host_api.h` instead of downloading it.
`-DRS64AP_REQUIRE_SSL=ON` fails the build when OpenSSL is missing; without OpenSSL the client only reaches `ws://`
servers.

## Installing and playing

1. Put `rs64.apworld` in Archipelago's `custom_worlds` folder (or double-click it).
2. Copy the `rs64ap` folder into the game's `mods/` folder.
3. Start the game, press **F6**, enable **Archipelago (RS64AP)**, and set **Server**, **Slot name** and **Password** in
   its settings. Restart the game.
4. Main menu → **ARCHIPELAGO** → pick a **fresh pilot** → **CONNECT**. The pilot is bound to that seed, and the client
   rewrites its unlocks, so don't use a pilot you care about. Next time, choosing the same pilot reconnects by itself.

The player guide is [apworld/rs64/docs/setup_en.md](apworld/rs64/docs/setup_en.md).

## Testing

```sh
# apworld: from an Archipelago source checkout with requirements installed
ln -s "$PWD/apworld/rs64" <Archipelago>/worlds/rs64
cd <Archipelago> && python -m pytest worlds/rs64/test

# client, without the game: build with the harness, then
cmake -S client -B build/client -DRS64AP_BUILD_HARNESS=ON && cmake --build build/client
python tools/e2e.py --ap <Archipelago> --build build/client
```

**Testing in the game** is the next step. Run the recomp with the mod enabled and keep its log (stderr). The
`[rs64ap] probe` lines answer the open spikes: craft masks per mission, pickup bits, objective counts.

## License

- `client/` is **GPL-3.0-or-later**. It's a plugin loaded into RogueSquadron64Recomp (GPL-3.0) and built against its
  header. See [client/NOTICE.md](client/NOTICE.md) for bundled third-party licenses.
- Everything else (apworld, data, tools, docs) is **MIT** ([LICENSE](LICENSE)), the usual license for Archipelago
  worlds.

You need your own legally obtained copy of the game. This project isn't affiliated with Lucasfilm, Nintendo,
Factor 5 or the recomp's authors.
