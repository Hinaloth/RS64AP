"""Generate solo seeds across the option combinations that change the item/location balance."""
import itertools
import unittest
from argparse import Namespace

from BaseClasses import CollectionState, MultiWorld
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
        )
        for goal, medals, pickups, starts, pool in combos:
            for seed in range(5):
                with self.subTest(goal=goal, medals=medals, pickups=pickups, starts=starts, pool=pool, seed=seed):
                    mw = generate(seed, goal=goal, medal_checks=medals, upgrade_pickups=pickups,
                                  starting_missions=starts, starting_mission_pool=pool)
                    self.assertTrue(mw.can_beat_game(CollectionState(mw)))
                    self.assertTrue(mw.has_beaten_game(mw.get_all_state()))
