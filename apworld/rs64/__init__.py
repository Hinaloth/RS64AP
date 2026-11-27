"""Archipelago world for Star Wars: Rogue Squadron (N64), played on RogueSquadron64Recomp.

Design and build notes: docs/BIBLE.md in the RS64AP repository.
"""
import logging
from typing import Any, ClassVar

from BaseClasses import ItemClassification, Region, Tutorial
from Options import OptionError
from rule_builder.rules import Has, HasAny, HasFromList, True_
from worlds.AutoWorld import WebWorld, World

from . import data
from .items import (EXTRAS, FILLER, FILLER_WEIGHTS, ITEM_GROUPS, LENIENCY, MISSION_CLEARED, MISSION_PASSES, SHIPS,
                    UPGRADES, VICTORY, RS64Item, base_classification)
from .locations import LOCATION_GROUPS, RS64Location, complete, medal, objective, pickup
from .options import RS64Options

logger = logging.getLogger("RS64")

CALAMARI = 15


class RS64Web(WebWorld):
    theme = "partyTime"
    tutorials = [Tutorial(
        "Multiworld Setup Guide",
        "Setting up Star Wars: Rogue Squadron (RogueSquadron64Recomp) for Archipelago.",
        "English",
        "setup_en.md",
        "setup/en",
        ["RS64AP contributors"],
    )]


class RS64World(World):
    """Fly Rogue Squadron's missions out of order: Mission Passes open missions, ships come one by one, and
    medals, completions and hidden upgrades send items to the multiworld."""

    game = data.GAME
    web = RS64Web()
    options_dataclass = RS64Options
    options: RS64Options
    origin_region_name = "Menu"

    item_name_to_id: ClassVar[dict[str, int]] = dict(data.ITEM_IDS)
    location_name_to_id: ClassVar[dict[str, int]] = dict(data.LOCATION_IDS)
    item_name_groups = ITEM_GROUPS
    location_name_groups = LOCATION_GROUPS

    starting_missions: list[int]
    starting_ships: list[str]
    gating_ships: set[str]

    def mission_ships(self, mission: dict) -> list[str]:
        """Ships the logic accepts for a mission under the Ship Logic option."""
        if self.options.ship_logic == self.options.ship_logic.option_any_listed:
            return [mission["default_ship"], *mission["replay_ships"]]
        return [mission["default_ship"]]

    # ---- generation -------------------------------------------------------------------------------------------

    def generate_early(self) -> None:
        if self.options.objective_checks and any(m["objectives"] is None for m in data.MISSIONS):
            raise OptionError(f"{self.player_name}: Objective Checks is not available yet "
                              "(per-mission objective counts are still unknown). Turn it off.")
        if self.options.ship_logic == self.options.ship_logic.option_any_listed:
            logger.warning(f"RS64: {self.player_name} uses Ship Logic any_listed; flying a replay ship on a "
                           "mission's first visit is not confirmed in-game yet.")
        self.gating_ships = {s for m in data.MISSIONS for s in self.mission_ships(m)}

        pool = [m["id"] for m in data.MISSIONS
                if self.options.starting_mission_pool == self.options.starting_mission_pool.option_any
                or m["kind"] != "bonus"]
        if self.options.goal == self.options.goal.option_calamari:
            pool.remove(CALAMARI)

        self.starting_missions = self.random.sample(pool, int(self.options.starting_missions))
        self.starting_ships = []
        for level in self.starting_missions:
            allowed = self.mission_ships(data.MISSIONS[level])
            if not any(s in self.starting_ships for s in allowed):
                self.starting_ships.append(self.random.choice(allowed))

        # One starting mission leaves only its own few checks reachable: make one of them open a second mission the
        # starting ship can fly, so solo seeds with tight settings still generate.
        if len(self.starting_missions) == 1:
            flyable = [m["id"] for m in data.MISSIONS
                       if m["id"] not in self.starting_missions and m["id"] in pool
                       and any(s in self.starting_ships for s in self.mission_ships(m))]
            if flyable:
                self.multiworld.local_early_items[self.player][MISSION_PASSES[self.random.choice(flyable)]] = 1

        for level in self.starting_missions:
            self.multiworld.push_precollected(self.create_item(MISSION_PASSES[level]))
        for ship in self.starting_ships:
            self.multiworld.push_precollected(self.create_item(ship))

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        hangar = Region("Hangar", self.player, self.multiworld)
        self.multiworld.regions += [menu, hangar]
        menu.connect(hangar)

        medal_tiers = int(self.options.medal_checks)
        gold_rule = (HasFromList(*UPGRADES, count=int(self.options.upgrades_for_gold))
                     if self.options.upgrades_for_gold > 0 else True_())

        for m in data.MISSIONS:
            region = Region(m["name"], self.player, self.multiworld)
            self.multiworld.regions.append(region)
            self.create_entrance(hangar, region, Has(MISSION_PASSES[m["id"]]) & HasAny(*self.mission_ships(m)),
                                 name=f"Launch {m['name']}")

            self._add_location(region, complete(m))
            for tier in range(1, medal_tiers + 1):
                loc = self._add_location(region, medal(m, tier))
                if tier == 3:
                    self.set_rule(loc, gold_rule)
            if self.options.upgrade_pickups and m["pickup"]:
                self._add_location(region, pickup(m))
            if self.options.objective_checks:
                for n in range(1, (m["objectives"] or 0) + 1):
                    self._add_location(region, objective(m, n))

            if self.options.goal == self.options.goal.option_missions:
                self._add_event(region, f"{m['name']} - Cleared", MISSION_CLEARED)
            elif m["id"] == CALAMARI:
                self._add_event(region, f"{m['name']} - Victory", VICTORY)

    def create_items(self) -> None:
        started = {MISSION_PASSES[level] for level in self.starting_missions} | set(self.starting_ships)
        pool = [self.create_item(name) for name in MISSION_PASSES + SHIPS + UPGRADES + EXTRAS if name not in started]

        slots = len(self.multiworld.get_unfilled_locations(self.player))
        if len(pool) > slots:
            raise OptionError(f"{self.player_name}: {len(pool)} items but only {slots} locations.")
        # Filler: one Medal Leniency per mission first (as many as fit), then the generic filler.
        leniency = self.random.sample(LENIENCY, min(len(LENIENCY), slots - len(pool)))
        pool += [self.create_item(name) for name in leniency]
        pool += [self.create_filler() for _ in range(slots - len(pool))]
        self.multiworld.itempool += pool

    def set_rules(self) -> None:
        if self.options.goal == self.options.goal.option_missions:
            self.set_completion_rule(Has(MISSION_CLEARED, count=int(self.options.missions_required)))
        else:
            self.set_completion_rule(Has(VICTORY))

    def fill_slot_data(self) -> dict[str, Any]:
        return {
            "world_version": data.WORLD_VERSION,
            "goal": int(self.options.goal),
            "missions_required": int(self.options.missions_required),
            "ship_logic": int(self.options.ship_logic),
            "medal_checks": int(self.options.medal_checks),
            "upgrade_pickups": int(self.options.upgrade_pickups),
            "objective_checks": int(self.options.objective_checks),
            "death_link": int(self.options.death_link),
        }

    # ---- items ------------------------------------------------------------------------------------------------

    def create_item(self, name: str) -> RS64Item:
        classification = base_classification(name)
        if not hasattr(self, "gating_ships"):
            self.gating_ships = {s for m in data.MISSIONS for s in self.mission_ships(m)}
        if name in SHIPS and name not in self.gating_ships:
            classification = ItemClassification.useful  # opens no mission under this Ship Logic
        if (name in UPGRADES and self.options.medal_checks == self.options.medal_checks.option_gold
                and self.options.upgrades_for_gold > 0):
            classification = ItemClassification.progression
        return RS64Item(name, classification, self.item_name_to_id[name], self.player)

    def get_filler_item_name(self) -> str:
        return self.random.choices(FILLER, weights=FILLER_WEIGHTS)[0]

    # ---- helpers ----------------------------------------------------------------------------------------------

    def _add_location(self, region: Region, name: str) -> RS64Location:
        loc = RS64Location(self.player, name, self.location_name_to_id[name], region)
        region.locations.append(loc)
        return loc

    def _add_event(self, region: Region, name: str, item: str) -> None:
        loc = RS64Location(self.player, name, None, region)
        region.locations.append(loc)
        loc.place_locked_item(RS64Item(item, ItemClassification.progression, None, self.player))
