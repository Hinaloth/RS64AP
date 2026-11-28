# Star Wars: Rogue Squadron Archipelago setup guide

## What you need

- **A Windows or Linux PC** that can run
  [RogueSquadron64Recomp](https://github.com/MikeSemicolonD/RogueSquadron64Recomp) v1.0.0 or newer. The Linux client builds and
  passes its tests but hasn't been played with yet, so expect rough edges there.
- **Your own copy of the game:** a dump of *Star Wars: Rogue Squadron* (USA v1.0) as `.z64`, `.v64` or `.n64`. It
  isn't provided here or by the recomp. Dump it from a cartridge you own.
- **[Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.8 or newer.**
- **From the [RS64AP releases](https://github.com/Hinaloth/RS64AP/releases):** `rs64.apworld`, `RS64AP.yaml`, and the
  client zip for your system (`rs64ap-client-windows.zip` or `rs64ap-client-linux.zip`).

## 1. Get the game running

Download RogueSquadron64Recomp **v1.0.0 or newer** for your system from its
[releases page](https://github.com/MikeSemicolonD/RogueSquadron64Recomp/releases) and unzip it wherever you like.

Put your ROM in the game's folder (next to the executable) under the name `rogue_squadron.z64`,
`rogue_squadron.v64` or `rogue_squadron.n64`, and launch. The game checks and imports it on the first launch. Play a
minute before adding the mod, to make sure the game itself works.

You can also build the recomp from source instead; its
[README](https://github.com/MikeSemicolonD/RogueSquadron64Recomp#building) explains how.

## 2. Install the client mod

1. Unzip the client zip into the game's `mods/` folder (next to the executable), so you end up with
   `mods/rs64ap/mod.json`.
2. Start the game, press **F6**, and in the **Mods** panel enable **Archipelago (RS64AP)**. Restart the game.
3. In the same Mods panel, keep the recomp's **Invincibility** and **Any Craft, Any Mission** mods off. They're off
   by default. Any Craft lets you fly ships you haven't received yet, and Invincibility stops DeathLink from killing
   you.

## 3. Install the apworld

Double-click `rs64.apworld`, or copy it into the `custom_worlds` folder of your Archipelago install. Restart the
Archipelago Launcher if it was open.

## 4. Make your YAML

Start from `RS64AP.yaml` from the release: every option in it has a comment. You can also get a fresh template from
the Archipelago Launcher (**Generate Template Options**). Set `name` to the slot name you'll play under, then
change the options you want.

## 5. Generate and host the seed

The archipelago.gg website can't generate seeds with custom worlds like this one, so generate on your own PC:

1. Put every player's YAML in the `Players` folder of your Archipelago install.
2. Run **Generate** from the Archipelago Launcher. The seed is written to the `output` folder as `AP_<numbers>.zip`.
3. Host it: upload that zip at [archipelago.gg/uploads](https://archipelago.gg/uploads) and create a room, or run
   **Host** from the Launcher to host it yourself.

Note the room's address and port (for example `archipelago.gg:38281`).

## 6. Connect

1. In the game, press **F6**, click **Archipelago (RS64AP)** to open its settings, and fill in:
   - **Server:** the room's address and port, for example `archipelago.gg:38281`.
   - **Slot name:** the `name` from your YAML.
   - **Password:** only if the room has one.
   - Leave **CA bundle** empty unless the connection fails with a certificate error.

   These fields can only be typed into in the F6 window, not in the game's own Mods menu.
2. From the main menu, choose **ARCHIPELAGO**. The game's pilot select opens, as it does for START: create a
   **new pilot**. The ARCHIPELAGO page then opens. Choose **CONNECT**, and when the page says CONNECTED the game
   carries on to mission select.

Use a fresh pilot for each seed. The first time a pilot connects, it's bound to that seed, and the client takes over
its unlocks, so don't use a pilot whose save you care about.

Next time, the same pilot reconnects by itself as long as the server address hasn't changed. With a new address, the
page waits for you to choose CONNECT. **FORGET PILOT SEED** unbinds a pilot if you want to reuse it for another seed.

A brand-new pilot is sent straight into Ambush at Mos Eisley by the game. If you don't hold that mission's pass,
abort from the pause menu: missions you don't have a pass for send no checks.

## If something goes wrong

- **"APWORLD VERSION MISMATCH":** the seed was generated with a different major version of the apworld than your
  client. Use the client and apworld from the same release, and regenerate the seed if needed.
- **"PILOT BOUND TO ANOTHER SEED":** that pilot belongs to an older seed. Pick a fresh pilot, or use **FORGET PILOT
  SEED**.
- **You can't type in the server fields:** use the F6 window, not the in-game Mods menu.
- **To report a bug**, open an issue on [the RS64AP repository](https://github.com/Hinaloth/RS64AP/issues). Attach
  `mod_data/rs64ap/client.log` from the game's folder, and the room's server log if a check or an item went missing.
