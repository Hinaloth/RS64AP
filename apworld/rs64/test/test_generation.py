from BaseClasses import ItemClassification

from .. import data
from ..items import EXTRAS, LENIENCY, MISSION_PASSES, UPGRADES
from . import RS64TestBase


def _location_count(world) -> int:
    return len([loc for loc in world.multiworld.get_locations(world.player) if loc.address is not None])


class TestDefaults(RS64TestBase):
    def test_location_count(self) -> None:
        # complete (19) + bronze, silver and gold (57, always) + pickups (10)
        pickups = sum(1 for m in data.MISSIONS if m["pickup"])
        self.assertEqual(_location_count(self.world), 19 * 4 + pickups)

    def test_gold_excluded_at_silver(self) -> None:
        from BaseClasses import LocationProgressType
        gold = self.multiworld.get_location("Ambush at Mos Eisley - Gold Medal", self.player)
        silver = self.multiworld.get_location("Ambush at Mos Eisley - Silver Medal", self.player)
        self.assertEqual(gold.progress_type, LocationProgressType.EXCLUDED)
        self.assertNotEqual(silver.progress_type, LocationProgressType.EXCLUDED)

    def test_items_fill_locations(self) -> None:
        own = [i for i in self.multiworld.itempool if i.player == self.player]
        self.assertEqual(len(own), _location_count(self.world))

    def test_starting_missions_are_playable(self) -> None:
        self.assertEqual(len(self.world.starting_missions), 2)
        state = self.multiworld.state
        for level in self.world.starting_missions:
            region = self.multiworld.get_region(data.MISSIONS[level]["name"], self.player)
            self.assertTrue(region.can_reach(state), data.MISSIONS[level]["name"])

    def test_calamari_never_starts(self) -> None:
        self.assertNotIn(15, self.world.starting_missions)

    def test_bonus_never_starts_in_campaign_pool(self) -> None:
        for level in self.world.starting_missions:
            self.assertNotEqual(data.MISSIONS[level]["kind"], "bonus")

    def test_upgrades_useful_without_gold(self) -> None:
        item = self.world.create_item(UPGRADES[0])
        self.assertEqual(item.classification, ItemClassification.useful)


class TestMinimum(RS64TestBase):
    options = {"medal_checks": "bronze", "upgrade_pickups": False, "starting_missions": 1}

    def test_fits(self) -> None:
        self.assertEqual(_location_count(self.world), 76)
        own = [i for i in self.multiworld.itempool if i.player == self.player]
        self.assertEqual(len(own), 76)
        non_filler = [i for i in own if i.name in MISSION_PASSES or i.name in UPGRADES or i.name in EXTRAS
                      or i.name in self.world.item_name_groups["Ships"]]
        self.assertLessEqual(len(non_filler), 35)


class TestGold(RS64TestBase):
    options = {"medal_checks": "gold", "upgrades_for_gold": 4}

    def test_gold_needs_upgrades(self) -> None:
        mission = data.MISSIONS[self.world.starting_missions[0]]
        gold = f"{mission['name']} - Gold Medal"
        self.assertFalse(self.can_reach_location(gold))
        self.collect_by_name(UPGRADES[:3])
        self.assertFalse(self.can_reach_location(gold))
        self.collect_by_name(UPGRADES[3:4])
        self.assertTrue(self.can_reach_location(gold))

    def test_upgrades_progression(self) -> None:
        self.assertEqual(self.world.create_item(UPGRADES[0]).classification, ItemClassification.progression)


class TestMissionsGoal(RS64TestBase):
    options = {"goal": "missions", "missions_required": 5, "starting_mission_pool": "any", "starting_missions": 3}

    def test_events(self) -> None:
        events = [loc for loc in self.multiworld.get_locations(self.player) if loc.address is None]
        self.assertEqual(len(events), 19)


class TestDeathLinkSlotData(RS64TestBase):
    options = {"death_link": True}

    def test_slot_data(self) -> None:
        slot = self.world.fill_slot_data()
        self.assertEqual(slot["death_link"], 1)
        self.assertEqual(slot["world_version"], data.WORLD_VERSION)


class TestShipLogicDefault(RS64TestBase):
    def test_kile_needs_y_wing(self) -> None:
        if "Y-wing" in self.world.starting_ships:
            self.skipTest("this seed starts with the Y-wing")
        self.collect_by_name(["Assault on Kile II Pass", "X-wing", "Millennium Falcon"])
        self.assertFalse(self.can_reach_region("Assault on Kile II"))
        self.collect_by_name(["Y-wing"])
        self.assertTrue(self.can_reach_region("Assault on Kile II"))

    def test_replay_only_ships_are_useful(self) -> None:
        for ship in ("Millennium Falcon", "TIE Interceptor", "Naboo N-1 Starfighter"):
            self.assertEqual(self.world.create_item(ship).classification, ItemClassification.useful)
        for ship in ("X-wing", "Y-wing", "A-wing", "V-wing", "Snowspeeder", "T-16 Skyhopper"):
            self.assertEqual(self.world.create_item(ship).classification, ItemClassification.progression)

    def test_starting_ships_are_defaults(self) -> None:
        defaults = {data.MISSIONS[level]["default_ship"] for level in self.world.starting_missions}
        self.assertEqual(set(self.world.starting_ships), defaults)


class TestShipLogicAny(RS64TestBase):
    options = {"ship_logic": "any_listed"}

    def test_falcon_opens_kile(self) -> None:
        self.collect_by_name(["Assault on Kile II Pass", "Millennium Falcon"])
        self.assertTrue(self.can_reach_region("Assault on Kile II"))

    def test_single_ship_missions_unchanged(self) -> None:
        for m in data.MISSIONS:
            if not m["replay_ships"]:
                self.assertEqual(self.world.mission_ships(m), [m["default_ship"]], m["name"])

    def test_all_ships_progression(self) -> None:
        self.assertEqual(self.world.create_item("Naboo N-1 Starfighter").classification,
                         ItemClassification.progression)


class TestNewItems(RS64TestBase):
    def test_replenishing_in_pool_and_useful(self) -> None:
        names = [i.name for i in self.multiworld.itempool if i.player == self.player]
        self.assertEqual(names.count("Replenishing Secondaries"), 1)
        self.assertEqual(self.world.create_item("Replenishing Secondaries").classification, ItemClassification.useful)

    def test_every_leniency_once_at_default(self) -> None:
        names = [i.name for i in self.multiworld.itempool if i.player == self.player]
        for name in LENIENCY:
            self.assertEqual(names.count(name), 1, name)
            self.assertEqual(self.world.create_item(name).classification, ItemClassification.filler)

    def test_invincibility_is_filler(self) -> None:
        self.assertEqual(self.world.create_item("Temporary Invincibility").classification, ItemClassification.filler)
