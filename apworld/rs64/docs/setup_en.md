# Star Wars: Rogue Squadron setup guide

## Required software

- [RogueSquadron64Recomp](https://github.com/MikeSemicolonD/RogueSquadron64Recomp) and your own
  `rogue_squadron.z64` (USA v1.0).
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.8 or newer.
- From the [RS64AP releases](https://github.com/Hinaloth/RS64AP/releases): `rs64.apworld` and the client mod zip for
  your system.

## Installing

1. Double-click `rs64.apworld`, or put it in Archipelago's `custom_worlds` folder.
2. Unzip the client mod into the game's `mods/` folder, so you have `mods/rs64ap/mod.json`.
3. Start the game, press **F6**, enable **Archipelago (RS64AP)** in the Mods window and restart the game.

## Making a YAML

Generate a template from the Archipelago Launcher (Generate Template Options), or start from `RS64AP.yaml` in the
release zip.

## Connecting

1. Press **F6**, open the RS64AP mod's settings and enter the server (`archipelago.gg:12345`), your slot name and the
   password, if there is one.
2. On the main menu, choose **ARCHIPELAGO**, then **create a new pilot**. A pilot is bound to one seed the first time it
   connects; use a fresh pilot for every seed, because the client rewrites the pilot's unlocks.
3. Choose **CONNECT** on the ARCHIPELAGO page. Once it says CONNECTED the game continues to mission select. The
   page reconnects by itself only for a pilot going back to the server it last played on; for a new server it waits
   for CONNECT, so **FORGET PILOT SEED** is there if you want to unbind the pilot first.

A brand-new pilot is sent straight into Ambush at Mos Eisley by the game. Abort it from the pause menu if you don't
hold that mission's pass: missions you don't hold a pass for send no checks.

## Playing

- Missions without a pass, or without a ship you own that the mission allows, can't be launched: the hangar refuses
  every ship and the status line says why. Back out with B.
- Received items show as a message in missions and on the status line in menus.
