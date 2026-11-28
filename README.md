# RS64AP

## What is RS64AP?

Rogue Squadron 64 Archipelago is a randomizer for
[RogueSquadron64Recomp](https://github.com/MikeSemicolonD/RogueSquadron64Recomp). It makes missions, ships and
upgrades into items to be found in the game (or games, if played in a multiworld). Completing missions, earning
medals, picking up upgrades and completing objectives are locations in which an item may be found.

To find out what Archipelago is, please visit [archipelago.gg](https://archipelago.gg).

Setup is in [SETUP.md](SETUP.md).

## Goal

To finish the AP, you have two options:

- `calamari` is the default. Complete the mission "The Battle of Calamari" and you'll have won your game.
- `missions` requires you to complete a set number of missions, chosen during the creation of your YAML.

## How a game plays

- You start with between 1 and 3 missions open (depending on your YAML options), each with a ship that can fly it
  (unless a ship you already start with can). Note that `starting_mission_pool: campaign` will only draw campaign
  missions to start from; `any` can also draw from the bonus missions.
- Every mission is listed in the mission select menu, but you won't be able to launch one without its pass. A status
  line above the mission icon says "NO MISSION PASS" or "NO SHIP FOR THIS MISSION" when a mission is currently
  unavailable. Bonus missions open with their pass item, no passcode is needed.
- The hangar will show every ship, but you will only be able to use the ones you have received as items, and only
  those your ship logic counts for that mission. Any attempt to select a ship not available will play a buzz. The
  mission select screen shows all the ships as possible even if not unlocked yet, rather than what you actually own
  during your run.

Ship logic is as such:

- `default_only` (default): a mission is in logic only with its default ship.
- `any_listed`: allows you to run a mission with any unlocked ship compatible with the mission in vanilla, even on a
  first visit. Some of them make a mission much harder.
- Default ships:
  - X-wing: Mos Eisley, Barkhesh, Gerrard V, Jade Moon, Rescue on Kessel, Prisons of Kessel, Chandrila, Seerdon's
    Revenge, Trench Run.
  - A-wing: Nonnah, Taloraan.
  - Y-wing: Kile II, Sullust.
  - V-wing: Calamari.
  - Snowspeeder: Corellia, Construction Yards, Fest, Hoth.
  - T-16 Skyhopper: Beggar's Canyon.

Some missions only allow for one ship to be used because of objectives that wouldn't be completable without them:
Corellia, Construction Yards, Fest and Hoth (Snowspeeder); Rescue on Kessel and Trench Run (X-wing); Beggar's
Canyon (T-16).

## Locations

- "Completion" sends upon finishing a mission to the end. This is the default type of location sent.
- "Medals" are given out by obtaining a certain medal on a mission. Bronze, silver and gold each give one check, and
  retroactively give the lower ranks as well. `medal_checks` picks the highest medal that can hold progression or
  useful items; medals above it only hold filler. Bronze is the minimal option and still ensures that a solo seed
  can receive every progression item.
- "Upgrade pickups" sends a check when you pick up one of the upgrade boxes present in some of the missions. The box
  doesn't give you its upgrade anymore: the upgrades are items in the pool.
- "Objectives" are sent when you complete one of the objectives found in each mission's pause menu.

## Items

- "Mission Pass" unlocks the mission it's named after. These items are marked as Progression.
- "Ship" unlocks the ship it's named after. These items are marked as Progression, except for ships no mission needs
  under your ship logic (with `default_only`: the Millennium Falcon, TIE Interceptor and Naboo N-1), which are
  marked as Useful.
- "Upgrades" give you one of the upgrades of the game. These items are marked as Useful as they give a real boost
  (Progression if `medal_checks: gold` and `upgrades_for_gold` asks for upgrades before a gold medal). To note: this
  AP introduces a new upgrade, the "Replenishing Secondaries", which allows you to regain a secondary ammo every 15
  seconds.
- "Medal Leniency" is another new item, a filler, one per mission, that makes the named mission's medal requirements
  easier to meet. The time limit is extended by ×1.5; kills, accuracy and friendlies needed are reduced by ×⅔.
- "Filler" is common items used to fill out any remaining locations not used by your Progression or Useful items.
  These apply instantly upon getting them during your mission; any received during a menu will be applied upon
  starting/resuming a mission.
  - Extra Life (most common): grants you 1 extra life, up to 9.
  - Secondary Refill: refills your secondaries to full.
  - Temporary Invincibility: allows you to take no damage for 15 s. A received DeathLink will still kill you.
  - Hull Repair (rarest): refills your hull to full.
- When receiving an item, a text line will appear to say what and from whom.

## DeathLink

- `death_link` (off by default) sends a DeathLink when you're shot down in a mission. Aborting a mission doesn't
  send one.
- `death_link_trigger` gives you control of when you send a DeathLink: `every_life` (default) sends one with your
  every death; `game_over` sends only the death on your last life.
- Received DeathLinks will cause you to crash and explode as though you had been shot down, but only while you're
  flying a mission. Received deaths always cost a life, up to the number you have left.

## Known limitations

The Linux client has never been tested yet. If you do, please reach out and tell me.

## Trackers

Universal Tracker works if you wish to use it.

## Credits and licence

RogueSquadron64Recomp by MikeSemicolonD, apclientpp by black-sliver, and Archipelago.

The client and probe are GPL-3.0-or-later; everything else is MIT.

Working on RS64AP itself: see [DEVELOPMENT.md](DEVELOPMENT.md).

## AI usage

This APworld and mod client was written with Claude's help. If you wish to not use LLM-written APworlds, please feel
free to write your own. If you do, please feel free to send me a link so I can add it here to redirect people
towards your work.
