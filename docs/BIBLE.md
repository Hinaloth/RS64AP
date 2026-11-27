# RS64AP Build Bible

The single reference for building the Archipelago randomizer for
[Star Wars: Rogue Squadron 64 Recompiled](https://github.com/MikeSemicolonD/RogueSquadron64Recomp).
It covers the design, the two halves of the build (the apworld and the in-game client mod), the data
both halves share, the order to build it in, and what still has to be proven.

Status markers used throughout:

- **[DECIDED]** agreed design, build it this way.
- **[PROPOSED]** a default picked to keep moving; change it freely.
- **[SPIKE Sn]** depends on a fact about the game nobody has checked yet; see [Spikes](#11-spikes).

**Build status (2026-11-27).** M1 (apworld) is done and tested. The client for M2–M8 is written and passes an
end-to-end test against a real Archipelago server with a fake game (`tools/e2e.py`), but has not run inside the
recomp. Every spike in section 11 is still open; the client logs `[rs64ap] probe` lines that answer S1, S4, S6 and S7.

Facts about the game cite the recomp repo's docs and sources by path (snapshot: commit `222947ee`,
2026-11-26, host API v12). Addresses are KSEG0 (`0x80......`).

---

## Contents

1. [Overview](#1-overview)
2. [Game facts the design rests on](#2-game-facts-the-design-rests-on)
3. [Items](#3-items)
4. [Locations](#4-locations)
5. [Goal, start and options](#5-goal-start-and-options)
6. [Logic](#6-logic)
7. [The apworld](#7-the-apworld)
8. [The client mod](#8-the-client-mod)
9. [Shared data and IDs](#9-shared-data-and-ids)
10. [Repository layout and build](#10-repository-layout-and-build)
11. [Spikes](#11-spikes)
12. [Milestones](#12-milestones)
13. [Testing](#13-testing)
14. [Out of scope](#14-out-of-scope)
15. [Open decisions](#15-open-decisions)
16. [Decision log](#16-decision-log)

---

## 1. Overview

**What the player experiences.** They make a new pilot, connect to the Archipelago server, and land
on SELECT LEVEL with one or two missions open and a ship that can fly them. Finishing missions,
earning medals and grabbing upgrade pickups sends checks out into the multiworld. Mission passes,
ships and upgrades arrive from it. They win by finishing The Battle of Calamari or a set number of
missions.

**What gets built.**

| Part | Language | Job |
| --- | --- | --- |
| `apworld/rs64` | Python | Generates seeds: items, locations, regions, rules, options, slot data |
| `client/` (native mod) | C++ | Runs inside the recomp: talks to the AP server, enforces what the player owns, detects checks |
| `data/` | JSON | One table of missions, craft, upgrades and IDs that both halves are generated from |

**Core principle: Archipelago owns progress, the game owns performance.** Everything the vanilla
game would *unlock* (missions, bonus levels, craft, upgrades) is derived from items received and
written over the game's own values. Everything the player *achieves* (completions, medals,
pickups, objectives) is read from the game and sent as checks. The client never tries to catch
every vanilla unlock path (medals, passcodes, debrief rewards); it overwrites the result. See
[8.4 Enforcement](#84-enforcement-derive-dont-intercept).

---

## 2. Game facts the design rests on

### 2.1 Missions (level ids)

From `src/main/discord_presence.cpp` and `tools/debug/recordings/README.md`.

| Id | Mission | Kind | Upgrade pickup |
| ---: | --- | --- | --- |
| 0 | Ambush at Mos Eisley | campaign | |
| 1 | Rendezvous on Barkhesh | campaign | |
| 2 | The Search for the Nonnah | campaign | yes |
| 3 | Defection at Corellia | campaign | yes |
| 4 | Liberation of Gerrard V | campaign | |
| 5 | The Jade Moon | campaign | yes |
| 6 | Imperial Construction Yards | campaign | |
| 7 | Assault on Kile II | campaign | yes |
| 8 | Rescue on Kessel | campaign | |
| 9 | Prisons of Kessel | campaign | yes |
| 10 | Battle Above Taloraan | campaign | yes |
| 11 | Escape from Fest | campaign | yes |
| 12 | Blockade on Chandrila | campaign | yes |
| 13 | Raid on Sullust | campaign | yes |
| 14 | Moff Seerdon's Revenge | campaign | yes |
| 15 | The Battle of Calamari | campaign (final) | |
| 16 | Beggar's Canyon | bonus | |
| 17 | The Death Star Trench Run | bonus | |
| 18 | The Battle of Hoth | bonus | |
| 20 | CHICKEN (AT-ST passcode level) | not in AP | |

The pickup column comes from the recordings README, which lists 10 levels whose run collects or
passes its level's upgrade. There are only 8 upgrade bits, so some levels must share an upgrade
type, or a pickup gives something other than an upgrade bit. **[SPIKE S4]**

### 2.2 Craft

`enum PlayerCraft` (`docs/reference/data-structures.md`, `gGameSettings+0x01`):

| Id | Craft | Hangar mask bit |
| ---: | --- | --- |
| 0 | X-wing | `0x001` |
| 1 | Y-wing | `0x002` |
| 2 | A-wing | `0x004` |
| 3 | V-wing | `0x008` |
| 4 | Snowspeeder | `0x010` |
| 5 | Millennium Falcon | `0x020` |
| 6 | TIE Interceptor | `0x040` |
| 7 | T-16 Skyhopper | `0x080` |
| 8 | Naboo N-1 Starfighter | `0x100` |

- `getAvailablePlayerCraftFlagsConsiderUnlocks(level)` is the hangar's mask;
  `getAvailablePlayerCraftFlagsIgnoreUnlocks(level)` is the icon list on SELECT LEVEL. Bits above
  `0x7F` come only from the level's default craft (`1 << table[level]`, table at `0x8009EC50`)
  (`mods/any-craft/src/any_craft.c`).
- Both functions carry a base `[[patches.hook]]` (mod-level craft rules, `rogue_squadron.toml`),
  so a `.nrm` `RECOMP_PATCH` of them is refused (`docs/guides/modding.md`, "Functions the game
  already patches"). The `any-craft` mod does this and is not shipped in `mods/platforms.json`.
  The client therefore gates craft at the hangar's own RAM mask instead (see 8.4).
- The hangar accepts A/Start only when the craft's bit is in the allowed mask at `0x800CD6D8`; a
  refused press plays the deny sound (`mods/multiplayer-native/src/session/ghost_lobby.cpp`, line
  ~1750). Bay → craft table `0x800CC3F8`, current bay `0x800CD6E4`.
- Naboo N-1: no flag bit. It is unlocked by a key, the CRC `0x050FE09C`, in
  `gGameSettings+0x28` and save body `+0xAC`; the hangar and model loader decrypt the ship with it.
- Falcon / TIE Interceptor: cheat bits `0x4` (FARMBOY) / `0x8` (TIEDUP) in `gActiveCheatFlags`
  (`0x80130B58`), or medal unlocks in the unlock word. Which missions offer them once unlocked is
  **[SPIKE S1]**.

### 2.3 Upgrades

Pilot unlock word (`account_data+0x0C`) and in-mission settings word `0x80130B4C`
(`gGameSettings+0x0C`), upgrade mask `0x0001FE00` (`tools/web/save-editor/save_editor.js`,
`mods/multiplayer-native/src/net/net_core.h`):

| Bit | Upgrade |
| --- | --- |
| `0x00200` | Advanced Lasers |
| `0x00400` | Advanced Concussion Missiles |
| `0x00800` | Seeker Concussion Missiles |
| `0x01000` | Advanced Proton Torpedoes |
| `0x02000` | Seeker Proton Torpedoes |
| `0x04000` | Seeker Cluster Missiles |
| `0x08000` | Advanced Bombs |
| `0x10000` | Advanced Shields |

- A pickup ORs its bit into `0x80130B4C` (`npcPowerUpUpdate` `0x800EBDB0`–`0x800EBE4C`). The game
  saves the bits to the pilot when the mission succeeds and reverts them otherwise
  (`docs/guides/multiplayer.md`).
- `unlockMissleUpgradeOnLevelCompletion` exists: at least one upgrade is granted on finishing a
  mission, not picked up. **[SPIKE S5]**

### 2.4 Mission state, objectives, medals, save

From `docs/reference/data-structures.md`:

- `gGameSettings` `0x80130B40` (0x30): `+0x00` current level, `+0x01` craft, `+0x0C` unlock /
  config word, `+0x18` cheat flags, `+0x28` Naboo key.
- `gMissionState` `0x80130B10` (0x28): `+0x00` numLives (default 3), `+0x02`/`+0x03` secondary
  weapon count / max, `+0x05` completedObjectiveFlags, `+0x06` hiddenObjectiveFlags, `+0x07`
  numMissionObjectives.
- `account_data` (0x14 per pilot, base `0x8013A5D4`): `+0x05` current_level (SELECT LEVEL lists
  campaign levels up to `min(this, 15)`), `+0x06` level_medals (2 bits × 19 levels: none / bronze /
  silver / gold), `+0x0C` unlock word.
- Save: 256-byte EEPROM image, two checksummed copies. The repo's save editor in
  `tools/web/save-editor` understands it.
- Cheat bits that bypass logic: `0x1` GAMEFLO! (all missions), `0x4` FARMBOY (Falcon), `0x8`
  TIEDUP (TIE), `0x10` HALIFAX? → `!YNGWIE!` (Naboo), `0x8000000` WOISTHAN (Trench Run),
  `0x10000000` DEADDACK (Hoth), `0x2000000` CHICKEN (AT-ST level).

### 2.5 Front-end facts

From `ghost_lobby.cpp` and `docs/engine/modding-host-api.md`:

- Mission select: cursor `0x800CDAC1`, screen state `0x800CDAC4`, **unlocked mask `0x800CDAD0`**,
  result `0x800CDACC`. `RS64_HOOK_MISSION_SELECT_INIT` fires before its init call with `a2` =
  highest level shown and `a3` = start level. `RS64_HOOK_MISSION_CONFIRMED` gives `s0` = the
  confirmed level.
- Pad: the pressed-buttons word `0x8013A960 + 4*port` can be edited in `RS64_HOOK_MENU_INPUT` /
  `RS64_HOOK_MISSION_SELECT_TICK` / `RS64_HOOK_CRAFT_SELECT_TICK`.
- Game state ids (`state_id()`): `menu.main`, `menu.select_game`, `menu.level_select`, `mission`,
  and others. `in_cutscene()` is non-zero during cinematics.

---

## 3. Items

### 3.1 Pool **[DECIDED]**

| Group | Count | Classification | Notes |
| --- | ---: | --- | --- |
| Mission passes | 19 | progression | One per mission, bonus missions included |
| Ships | 9 | progression | X-wing, Y-wing, A-wing, V-wing, Snowspeeder, Falcon, TIE Interceptor, T-16, Naboo N-1 |
| Upgrades | 8 | useful | One per upgrade bit, not progressive |
| Filler | rest | filler | Only applied during an active mission |
| Traps | 0 | | None |

Starting missions and starting ships come out of the pool as precollected items.

### 3.2 Names

- Passes: `<Mission name> Pass`, e.g. `Ambush at Mos Eisley Pass`, `The Battle of Hoth Pass`.
- Ships: `X-wing`, `Y-wing`, `A-wing`, `V-wing`, `Snowspeeder`, `Millennium Falcon`,
  `TIE Interceptor`, `T-16 Skyhopper`, `Naboo N-1 Starfighter`.
- Upgrades: the names in [2.3](#23-upgrades).
- Item groups: `Mission Passes`, `Campaign Passes`, `Bonus Passes`, `Ships`, `Upgrades`.

### 3.3 Filler **[PROPOSED]**

Received filler waits in a persistent queue and is applied only when the player is in an active
mission: state `mission`, after `RS64_HOOK_MISSION_READY`, not `in_cutscene()`, not paused. One
item per second so each gets its own message.

| Item | Effect | Weight |
| --- | --- | ---: |
| Extra Life | `numLives += 1` (cap 9) | 3 |
| Secondary Refill | secondary count = max (`gMissionState+0x02` = `+0x03`) | 2 |
| Hull Repair | restore the player craft's health | 1 |

Whether lives carry between missions changes how Extra Life feels **[SPIKE S8]**. Hull Repair needs
the craft health field **[SPIKE S8]**; drop it if that takes too long.

---

## 4. Locations

### 4.1 Sets

| Set | Per mission | Total | Default | Option |
| --- | --- | ---: | --- | --- |
| Mission complete | 1 | 19 | always | none |
| Medals | 1–3 (cumulative) | 19 / 38 / 57 | silver (38) | `medal_checks`: bronze / silver / gold, never off **[DECIDED]** |
| Upgrade pickups | 0–1 | 10 | on | `upgrade_pickups` (encouraged) **[DECIDED]** |
| Objectives | n | TBD | off | `objective_checks` **[DECIDED]**; counts per mission are **[SPIKE S6]** |

- **Medals are cumulative.** At `silver`, earning gold sends bronze and silver; earning silver
  sends bronze and silver. A later, better run sends the missing tiers. This assumes
  `level_medals` keeps the best medal ever earned **[SPIKE S7]**; if it keeps the latest, the client
  tracks the best itself.
- **Bonus missions** have medals too (`level_medals` covers 19 levels).

### 4.2 Names

`<Mission name> - Complete`, `- Bronze Medal`, `- Silver Medal`, `- Gold Medal`,
`- Upgrade Pickup`, `- Objective <n>`.

### 4.3 Item / location budget

Worst case for items: one starting mission and one starting ship:
18 passes + 8 ships + 8 upgrades = **34** non-filler items.

| Settings | Locations | Spare for filler |
| --- | ---: | ---: |
| Minimum: complete + bronze | 38 | 4 |
| Default: complete + silver + pickups | 67 | 33 |
| Max (no objectives): complete + gold + pickups | 86 | 52 |

The minimum always fits, which is why `medal_checks` has no off. Each extra starting mission takes
one pass, and sometimes one ship, out of the pool. The apworld still asserts
`locations >= items` in `create_items` and fills the gap with filler.

---

## 5. Goal, start and options

### 5.1 Goal **[DECIDED]**

- `goal: calamari` (default): complete The Battle of Calamari.
- `goal: missions`: complete `missions_required` missions (1–19, default 10), any mix of campaign
  and bonus.

Completion is reported with `StatusUpdate(CLIENT_GOAL)` when the goal's condition becomes true,
checked by the client against its own sent-location set.

### 5.2 Start **[DECIDED]**

- `starting_missions` passes are precollected, drawn from `starting_mission_pool`.
- For each starting mission, if no ship owned so far can fly it, one ship from its allowed set is
  precollected (random). Two starting missions can therefore share one starting ship.
- With `goal: calamari`, Calamari is never a starting mission.
- If the T-16 or Snowspeeder missions are drawn, their only craft becomes the starting ship.

### 5.3 Options

| Option | Values | Default | Status |
| --- | --- | --- | --- |
| `goal` | `calamari`, `missions` | `calamari` | DECIDED |
| `missions_required` | 1–19 | 10 | DECIDED (range), default PROPOSED |
| `starting_missions` | 1–3 | 2 | PROPOSED (see Open decisions) |
| `starting_mission_pool` | `campaign`, `any` | `campaign` | PROPOSED |
| `medal_checks` | `bronze`, `silver`, `gold` | `silver` | DECIDED |
| `upgrade_pickups` | on / off | on | DECIDED |
| `objective_checks` | on / off | off | DECIDED |
| `upgrades_for_gold` | 0–8 | 4 | PROPOSED |
| `death_link` | on / off | off | PROPOSED, milestone M8 |

Standard AP options (`start_inventory`, `local_items`, `exclude_locations`, ...) come from
`PerGameCommonOptions`.

---

## 6. Logic

### 6.1 Regions

```
Menu ──► Hangar ──► <Mission 0> … <Mission 18>
```

- `Menu` → `Hangar`: free.
- `Hangar` → `<Mission N>`: `Has(<N> Pass) & HasAny(*allowed_ships[N])`.
- All of a mission's locations live in its region.

`allowed_ships[N]` is the vanilla hangar mask for level N with every craft unlocked, from
**[SPIKE S1]**. Until S1 lands, the table is filled from the vanilla defaults known to be single-craft
(Hoth: Snowspeeder; Beggar's Canyon: T-16) and marked provisional in `data/`.

### 6.2 Location rules

| Location | Extra rule beyond region access |
| --- | --- |
| Complete | none |
| Bronze | none |
| Silver | none **[PROPOSED]** |
| Gold | `HasFromList(*UPGRADES, count=upgrades_for_gold)` **[PROPOSED]** |
| Upgrade Pickup | none |
| Objective n | none |

Bronze counts as reachable together with the mission. It isn't automatic, but it's close.

### 6.3 Goal rule

Events are separate locations (no ID) in the mission's region, with the same rule as its
`- Complete` location:

- `calamari`: event location `The Battle of Calamari - Victory` holds the locked event `Victory`;
  completion condition `Has("Victory")`.
- `missions`: each mission gets an event location `<Mission> - Cleared` holding a locked
  `Mission Cleared` event; completion condition `Has("Mission Cleared", count=missions_required)`.

### 6.4 Writing rules

Use the core rule builder (`rule_builder`: `Has`, `HasAny`, `HasAll`, `HasFromList`, combined
with `&` / `|`, assigned with `self.set_rule`), not lambdas. It caches, serializes and explains
itself to players (`docs/rule builder.md` in Archipelago).

---

## 7. The apworld

### 7.1 Layout

```
apworld/rs64/
  archipelago.json      {"game": "Star Wars Rogue Squadron", "minimum_ap_version": "0.6.8", "world_version": "0.1.0", "authors": [...]}
  __init__.py           RS64World
  items.py              item table, groups, filler weights
  locations.py          location table
  options.py            option classes + RS64Options dataclass
  regions.py            create_regions
  rules.py              rule-builder rules
  data.py               generated from data/rs64ap.json — do not edit
  docs/en_Star Wars Rogue Squadron.md
  docs/setup_en.md
  test/
```

- `minimum_ap_version`: `0.6.8`, the latest stable release in November 2026 and the first with everything used
  here (the rule builder). The spec says most worlds only need the minimum.
- Game name: `Star Wars Rogue Squadron` **[DECIDED]**, with no colon: AP names the game-info doc
  `en_<game>.md`, and a colon is not allowed in Windows file names. It must match the client's `Connect` packet
  exactly and can't change after release.
- A packaged `.apworld` also needs `version` / `compatible_version` (7) in its manifest; `tools/package.py`
  adds them.

### 7.2 World class

| Method | Does |
| --- | --- |
| `generate_early` | Validate options. Pick starting missions and ships. `push_precollected` them. |
| `create_regions` | Menu, Hangar, 19 mission regions; add locations per enabled set; add event locations. |
| `create_items` | Build the pool minus precollected; pad with filler from `get_filler_item_name`. |
| `set_rules` | Entrance and location rules from section 6; set the completion condition. |
| `fill_slot_data` | See 7.3. |
| `get_filler_item_name` | Weighted pick from the filler table. |

### 7.3 Slot data

```json
{
  "world_version": "0.1.0",
  "goal": 0,
  "missions_required": 10,
  "medal_checks": 1,
  "upgrade_pickups": 1,
  "objective_checks": 0,
  "death_link": 0
}
```

The client refuses to play a slot whose `world_version` major differs from its own.

---

## 8. The client mod

### 8.1 Form

A **native mod** (DLL / `.so`) using the host API, plus a `roguesq_menu.json`. Native, not `.nrm`,
because it needs networking, files and threads (`docs/engine/modding-host-api.md`). No `.nrm` is
needed if craft gating works through the hangar RAM mask **[SPIKE S3]**. If it doesn't, a small
companion `.nrm` with `RECOMP_FORCE_PATCH` is the fallback; it must carry the base hook's
mod-level behaviour itself.

- Exports `rs64_mod_init` and `uint32_t recomp_api_version = 1`; listed in `mod.json`
  `native_libraries`. Checks `table->size` and requires host API ≥ 12.
- Mod id `rs64ap`. Off by default (`enabled_by_default: false`).
- Template: `mods/native-demo` for the skeleton, `mods/multiplayer-native` for networking, the
  pilot-page flow, mission select and hangar hooks.

### 8.2 Threads

- **Network thread**: owns the AP client library and the socket. Never touches RDRAM.
- **Game thread**: every hook handler. Never blocks or sleeps (host API rule).
- Two lock-protected queues between them: `inbox` (received items, messages, deathlinks, connection
  state) and `outbox` (location checks, status updates, deathlinks). The game thread drains `inbox`
  once per `RS64_HOOK_FRAME`.

### 8.3 AP client library **[SPIKE S11]**

1. **APCpp** (C++, high-level: `AP_Init`, item / location callbacks, DeathLink support built in).
2. **apclientpp** (header-only C++14, needs nlohmann/json and asio; supports SSL).

Must handle `wss://` to archipelago.gg on Windows and Linux. Use `items_handling = 0b111` (remote,
own-world and starting-inventory items all sent to the client).

### 8.4 Enforcement: derive, don't intercept

The client keeps an **AP state**: received passes, ships, upgrades, Naboo flag. On every
`RS64_HOOK_FRAME` in menus, and at `RS64_HOOK_MISSION_INIT`, it writes the game's values from it:

| Game value | Written as |
| --- | --- |
| Pilot `current_level` | 15, so SELECT LEVEL builds every campaign entry |
| Mission-select unlocked mask `0x800CDAD0` | not written yet: the client blocks A/Start on a mission with no pass or no flyable ship instead (the fallback below), until S2 shows whether the mask can gate entries out of order **[SPIKE S2]** |
| Bonus-level visibility (cheat bits WOISTHAN / DEADDACK and the Beggar's Canyon bit) | set only for owned bonus passes **[SPIKE S2]** |
| Cheat bits GAMEFLO!, FARMBOY, TIEDUP, HALIFAX?, CHICKEN | cleared; FARMBOY / TIEDUP set only when Falcon / TIE received |
| Naboo key `gGameSettings+0x28` / save `+0xAC` | `0x050FE09C` when N-1 received, else 0 |
| Pilot unlock word (`kUnlockMask` `0x7FFE00`) | exactly the received upgrades, plus the all-craft bit `0x80000` so the hangar offers every craft a level allows; the other level/craft unlock bits cleared |
| `0x80130B4C` upgrade bits, during a mission | exactly the received upgrades, every mission frame (reverts pickups) |
| Hangar allowed mask `0x800CD6D8` | vanilla mask AND received ships, at `CRAFT_SELECT_INIT` and each `CRAFT_SELECT_TICK` **[SPIKE S3]** |

This makes vanilla unlock paths harmless without hooking each one: medal unlocks, debrief rewards,
`unlockMissleUpgradeOnLevelCompletion` and passcodes all get overwritten on the next frame.

**Fallback gating for missions.** If `0x800CDAD0` doesn't gate entries, in
`RS64_HOOK_MISSION_SELECT_TICK` read the cursor (`0x800CDAC1`) and clear A/Start from the pressed
word while it sits on an unowned mission. Show `NO MISSION PASS` on the status line.

**Save writes.** The game persists unlocks via `setUnlockBitAndPersist` / its save dispatcher. Writes
from enforcement are in-memory and get saved whenever the game saves next; that's fine, because
enforcement runs again after every load. Never touch the EEPROM image directly.

### 8.5 Check detection

| Check | Source | When it's sent |
| --- | --- | --- |
| Complete | `RS64_HOOK_RESULT_SUCCESS` sets `pending_complete[level]` | At `RS64_HOOK_MISSION_END` if still pending, or at debrief **[SPIKE S9]** |
| Medal | `level_medals` diff against last-sent, per level | On `STATE_CHANGED` out of the debrief, and on connect |
| Upgrade pickup | `RS64_HOOK_POWERUP_COLLECT` (`s1` = pickup, `s1+4` its DAT record) | Immediately; keyed by `(level, DAT record)` from the data table |
| Objective n | `completedObjectiveFlags` diff each mission frame | Immediately **[PROPOSED]**, even if the mission later fails |

- `pending_complete` is cleared by `RESULT_FAIL`, abort or restart.
- On every (re)connect, the client re-sends all locations it can derive from the save (medals,
  completions it recorded locally). Location checks are idempotent on the server.
- The client sends nothing while the multiplayer mod is in a session (`get_flag("mp_lobby_state")`
  non-zero) and shows `AP PAUSED IN CO-OP`.

### 8.6 Receiving items

- Passes, ships, upgrades: add to AP state; enforcement picks them up next frame. Safe in menus and
  in missions. An upgrade received mid-mission is live at once.
- Filler: append to `filler_queue`; drained as in [3.3](#33-filler-proposed).
- Every received item shows a message (8.8).

### 8.7 Persistence

`mod_data_dir("rs64ap")` + `save_slot()` → `pilot_<slot>.json`:

```json
{
  "seed_name": "…", "slot_name": "…", "server": "archipelago.gg:12345",
  "received_index": 42,
  "sent_locations": [640000, 640001],
  "pending_filler": ["Extra Life"],
  "pending_complete": []
}
```

- An AP pilot is bound to one seed. Connecting a pilot to a different seed is refused: `PILOT BOUND
  TO ANOTHER SEED`.
- `received_index` lets the client skip items it already applied when the server replays
  `ReceivedItems` on reconnect. AP state itself is rebuilt from the full item list every connect,
  so it can't drift.
- The password is not stored.

### 8.8 UI

**Connect flow** (copy the multiplayer lobby):

1. Main menu `ARCHIPELAGO` entry: a `main_menu` action with `"transition": "account"` and
   `page: "ap_connect"`. Choosing a pilot opens the page instead of mission select
   (`docs/guides/adding-menus-and-buttons.md`).
2. `ap_connect` page: `SERVER`, `SLOT`, `PASSWORD` rows edited through `add_key_handler` +
   `text_input`; `CONNECT` action; status label from a text source (`CONNECTING`, `CONNECTED`,
   `BAD SLOT`, ...).
3. `exit_when: ap_ready` (an `add_condition`) continues to mission select with the pilot loaded.

M2 can skip the page: read server and slot from the mod's `config_schema` via `config_string`
(editable in the F6 Mods window).

**Messages:**

- In a mission: `run_rule_line("message 3 RECEIVED ADVANCED SHIELDS FROM BOB")`.
- In menus: a status line on an extra font slot, as the lobby does on mission select (`y = -0x7C`)
  and craft select (`y = -0x70`).
- Menu font is uppercase and limited **[SPIKE S12]**: upper-case, strip or transliterate anything
  outside the charset, truncate long names from other games.

**Mission select hint:** owned passes vs missions with no flyable ship, e.g. `NO SHIP FOR THIS
MISSION` on the status line.

### 8.9 DeathLink (M8)

- Send: on a life lost (`numLives` decrement in a mission), cause = `<slot> was shot down over
  <mission>`.
- Receive: queued like filler; applied by calling `applyPlayerCraftDamage` with lethal damage through
  `call` from a mission hook.
- Never send a death caused by a received DeathLink.

---

## 9. Shared data and IDs

### 9.1 One source of truth

`data/rs64ap.json` holds missions, craft, upgrades, allowed ships per mission, pickup DAT keys,
objective counts, and all names and IDs. `tools/gen_data.py` writes `apworld/rs64/data.py` and
`client/src/generated/rs64ap_data.h`. Neither generated file is edited by hand; CI fails if they're
stale.

### 9.2 ID scheme **[PROPOSED]**

Base `640000` (`64` for RS64; AP IDs only need to be unique within a game).

**Items** `640000 + n`:

| Range | Items |
| --- | --- |
| 1–19 | Mission passes, `n = level + 1` |
| 101–109 | Ships, `n = 101 + craft id` |
| 201–208 | Upgrades, `n = 201 + bit index − 9` |
| 301+ | Filler |

**Locations** `640000 + level × 100 + slot`:

| Slot | Location |
| --- | --- |
| 0 | Complete |
| 1 / 2 / 3 | Bronze / Silver / Gold |
| 10 | Upgrade Pickup |
| 20–29 | Objective 1–10 |

IDs never change once released; new location kinds take unused slots.

---

## 10. Repository layout and build

```
RS64AP/
  README.md
  docs/BIBLE.md               this file
  data/rs64ap.json            shared table
  apworld/rs64/               Python world
  client/
    CMakeLists.txt
    mod.json / roguesq_menu.json
    src/                      mod sources
    src/generated/            from tools/gen_data.py
    third_party/              AP client lib + deps (submodules)
  probes/                     spike mods (native-demo copies that log facts)
  tools/
    gen_data.py
    package.py                .apworld + client zip
  .github/workflows/          lint, apworld tests, client build (Windows, Linux)
```

**Building the client.** Out of tree, against the recomp's public headers, the way
`mods/native-demo` builds standalone (`cmake -S <mod> -B build`). CMake takes
`-DRS64_RECOMP_DIR=<path to a RogueSquadron64Recomp checkout>` and adds its `include/`. The recomp
is a build-time path or submodule, never copied into this repo.

**Licensing [DECIDED].** The recomp is GPL-3.0 (its `LICENSE` and README; an earlier note here said it had none,
which was wrong). The client is a plugin loaded into that process and built against its GPL header, so `client/` is
**GPL-3.0-or-later**. The apworld runs inside Archipelago (MIT) and only talks to the client over the network, so it
and the rest of the repo are **MIT**, the norm for Archipelago worlds. Bundled client dependencies (apclientpp,
wswrap, websocketpp, asio, nlohmann/json, OpenSSL 3, zlib) are all GPL-3.0-compatible; `client/NOTICE.md` lists them.

**Release artifacts:** `rs64.apworld`, `rs64ap-client-<os>.zip` (the mod folder to drop into the
game's `mods/`), a template YAML.

---

## 11. Spikes

Each spike is a throwaway probe (a `native-demo` copy in `probes/` that logs), answers one question,
and writes its answer into `data/rs64ap.json` or this file. Drive the game with `ROGUESQ_BOOT_TARGET`
and replay the repo's recordings with `ROGUESQ_LS_REPLAY`
(`docs/engine/debug-trace-env-vars.md`).

| Id | Question | How | Blocks |
| --- | --- | --- | --- |
| S1 | Vanilla allowed craft per level with every craft unlocked (Falcon, TIE, N-1 included) | Call both craft-mask functions for levels 0–18 with all cheat bits + Naboo key set; log masks | Logic (M1 final) |
| S2 | Does writing `0x800CDAD0` (and `a2` at `MISSION_SELECT_INIT`) gate entries non-contiguously? How are bonus levels listed? | Write masks, look | M2 |
| S3 | Does ANDing `0x800CD6D8` at craft-select init / tick refuse craft cleanly? | Write mask, try picking | M3 |
| S4 | Every power-up per level: DAT record, upgrade bit; does it spawn if the bit is already owned? | Log `POWERUP_COLLECT` over the 7 recordings that collect; set bits and check spawns | M4 |
| S5 | What does `unlockMissleUpgradeOnLevelCompletion` grant, on which level? | Log the unlock word across a completion | M4 |
| S6 | `numMissionObjectives` and flag semantics per level; hidden objectives | Log `gMissionState+0x05..0x07` across recordings | M6 |
| S7 | When is `level_medals` written (debrief / save)? Best-ever or latest? Is a medal written on failure? | Log the account across debriefs | M5 |
| S8 | Do lives carry between missions? Where is player craft health? | Log `numLives` across missions; player struct | M7 |
| S9 | Is `RESULT_SUCCESS` final, or can a success request be undone? Abort / restart paths | Log result hooks and state changes | M2 |
| S10 | Does per-frame enforcement fight the game's own save writes, or the passcode screen? | Enforce while entering passcodes and saving | M2 |
| S11 | AP client library builds as a native mod and reaches `wss://archipelago.gg` on Windows + Linux | Minimal connect probe | M2 |
| S12 | Menu font charset | Render every byte 0x20–0x7F in a label | M8 |

---

## 12. Milestones

| M | Deliverable | Done when |
| --- | --- | --- |
| M0 | Spikes S1, S2, S3, S9, S11 | Answers recorded |
| M1 | apworld: all items, locations, options, logic; generates | Tests in 13.1 pass |
| M2 | Client: config connect, mission passes (enforcement + gating), Complete checks, goal | A solo seed can be won with completion checks only and passes forced on |
| M3 | Ship gating at the hangar; starting ships | Can't launch with an unowned ship |
| M4 | Upgrades enforced; pickup checks | Pickup sends its check and doesn't grant the upgrade |
| M5 | Medal checks | Bronze/silver/gold send correctly, cumulatively |
| M6 | Objective checks | Each objective sends once |
| M7 | Filler queue | Filler applies only in active missions, survives a restart |
| M8 | Connect page, messages, DeathLink | Playable without editing config |
| M9 | Release: docs, template YAML, CI artifacts; optional Universal Tracker / PopTracker support | Tagged release |

M2 is the first playable build.

---

## 13. Testing

### 13.1 apworld

Using Archipelago's `WorldTestBase`:

- Generation succeeds for every combination of `goal`, `medal_checks`, `upgrade_pickups`,
  `objective_checks`, `starting_missions` 1–3, `starting_mission_pool`; 50 seeds each, solo.
- Minimum settings (bronze, no pickups, 1 starting mission) generate with no forced failure.
- Every starting mission is reachable with the precollected items alone.
- `goal: calamari` never starts on Calamari.
- Item count ≤ location count in every combination.
- IDs in `data.py` match `data/rs64ap.json` (generation is reproducible).

### 13.2 Client

- **Replays**: the recordings in `tools/debug/recordings/` drive missions with fixed inputs. Use the
  pickup-collecting ones for M4 and the completions for M2/M5.
- **Local server**: Archipelago's `MultiServer.py` on localhost with a generated test seed; a second
  text client watches checks arrive.
- **Manual checklist per milestone**: connect, reconnect mid-mission, quit and relaunch, a pilot
  bound to another seed, server restart, passcodes typed.

---

## 14. Out of scope

- Traps **[DECIDED]**.
- The CHICKEN AT-ST level and the Tatooine race passcode (not in the pool; their cheat bits are
  cleared).
- Co-op Archipelago (client pauses during multiplayer sessions).
- Android and web builds (Android maybe later; web can't load native mods).
- Mod levels from other mods.
- Ranks (not used; "ranks" was another word for medals).

---

## 15. Open decisions

1. **`starting_missions` default**: 2 (gives 4 checks in sphere 1 for solo seeds) vs 1 (as first
   described).
2. **Gold logic**: require `upgrades_for_gold` upgrades (default 4), or no extra rule.
3. **Silver logic**: currently no extra rule.
4. **Objectives**: send immediately even if the mission fails (current), or only on success.
5. **Filler set**: keep Hull Repair if S8 finds the health field cheaply?

---

## 16. Decision log

| Date | Decision |
| --- | --- |
| 2026-11-27 | Mission passes are individual items, 19 of them, bonus missions included; one or more given at start. |
| 2026-11-27 | Ships are individual items; the starting ship matches the starting mission so it can be finished. |
| 2026-11-27 | Naboo N-1 is in the ship pool. |
| 2026-11-27 | T-16 Skyhopper is its own item (only Beggar's Canyon uses it). |
| 2026-11-27 | Upgrades are individual "useful" items, not progressive. |
| 2026-11-27 | Filler allowed, applied only during active missions. |
| 2026-11-27 | No traps. |
| 2026-11-27 | Locations: mission complete (always); objectives optional, off by default; upgrade pickups optional, encouraged; medals bronze/silver/gold, default silver, no off. |
| 2026-11-27 | No rank checks. |
| 2026-11-27 | Goal: The Battle of Calamari or X missions completed. |
| 2026-11-27 | Game name `Star Wars Rogue Squadron` (no colon). |
| 2026-11-27 | Licenses: `client/` GPL-3.0-or-later (follows the recomp), everything else MIT. |
| 2026-11-27 | `minimum_ap_version` 0.6.8. |
