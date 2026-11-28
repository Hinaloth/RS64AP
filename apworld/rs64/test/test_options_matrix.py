"""Generate solo seeds across the option combinations that change the item/location balance."""
import itertools
import unittest
from argparse import Namespace

from BaseClasses import CollectionState, LocationProgressType, MultiWorld
from Fill import distribute_items_restrictive
from worlds.AutoWorld import call_all

from .. import RS64World, data

STEPS = ("generate_early", "create_regions", "create_items", "set_rules", "connect_entrances", "generate_basic",
         "pre_fill")


def generate(seed: int, **opts) -> MultiWorld:
    mw = MultiWorld(1)
    mw.game[1] = data.GAME
    mw.player_name = {1: "Tester"}
    mw.set_seed(seed)
    args = Namespace()
    for name, option in RS64World.options_dataclass.type_hints.items():
        value = opts.get(name, option.default)
        setattr(args, name, {1: option.from_any(value)})
    mw.set_options(args)
    mw.state = CollectionState(mw)
    for step in STEPS:
        call_all(mw, step)
    distribute_items_restrictive(mw)
    call_all(mw, "post_fill")
    return mw


class TestMatrix(unittest.TestCase):
    def test_all_beatable(self) -> None:
        combos = itertools.product(
            ("calamari", "missions"),
            ("bronze", "silver", "gold"),
            (False, True),           # upgrade_pickups
            (1, 2, 3),               # starting_missions
            ("campaign", "any"),
            ("default_only", "any_listed"),
            (False, True),           # objective_checks
        )
        for goal, medals, pickups, starts, pool, ships, objectives in combos:
            for seed in range(2):
                with self.subTest(goal=goal, medals=medals, pickups=pickups, starts=starts, pool=pool, ships=ships,
                                  objectives=objectives, seed=seed):
                    mw = generate(seed, goal=goal, medal_checks=medals, upgrade_pickups=pickups,
                                  starting_missions=starts, starting_mission_pool=pool, ship_logic=ships,
                                  objective_checks=objectives)
                    self.assertTrue(mw.can_beat_game(CollectionState(mw)))
                    self.assertTrue(mw.has_beaten_game(mw.get_all_state()))
                    for loc in mw.get_locations(1):
                        if loc.progress_type == LocationProgressType.EXCLUDED:
                            self.assertFalse(loc.item.advancement or loc.item.useful, loc.name)
