from dataclasses import dataclass

from Options import Choice, DeathLink, DefaultOnToggle, PerGameCommonOptions, Range, StartInventoryPool, Toggle


class Goal(Choice):
    """What you need to do to win.

    calamari: complete The Battle of Calamari.
    missions: complete a number of missions (see Missions Required), campaign and bonus both count."""
    display_name = "Goal"
    option_calamari = 0
    option_missions = 1
    default = 0


class MissionsRequired(Range):
    """How many missions must be completed when Goal is missions."""
    display_name = "Missions Required"
    range_start = 1
    range_end = 19
    default = 10


class StartingMissions(Range):
    """How many Mission Passes you start with. Each comes with a ship that can fly it, unless a ship you
    already start with can."""
    display_name = "Starting Missions"
    range_start = 1
    range_end = 3
    default = 2


class StartingMissionPool(Choice):
    """Which missions can be drawn as starting missions.

    campaign: the 16 campaign missions (The Battle of Calamari is never drawn when it is the goal).
    any: bonus missions can be drawn too."""
    display_name = "Starting Mission Pool"
    option_campaign = 0
    option_any = 1
    default = 0


class MedalChecks(Choice):
    """The best medal tier that is a check. Medals count up: earning gold sends every enabled tier below it.

    bronze: one medal check per mission. silver: two. gold: three."""
    display_name = "Medal Checks"
    option_bronze = 1
    option_silver = 2
    option_gold = 3
    default = 2


class UpgradePickups(DefaultOnToggle):
    """The hidden upgrade pickup in each mission that has one is a check. The upgrade itself comes from the
    item pool instead."""
    display_name = "Upgrade Pickups"


class ObjectiveChecks(Toggle):
    """Each mission objective is a check. Adds many checks, most of them filler.
    Not available yet: needs the per-mission objective counts (spike S6)."""
    display_name = "Objective Checks"


class UpgradesForGold(Range):
    """How many upgrades the logic expects you to have before a Gold Medal check. 0 = no requirement.
    Only matters when Medal Checks is gold."""
    display_name = "Upgrades for Gold"
    range_start = 0
    range_end = 8
    default = 4


@dataclass
class RS64Options(PerGameCommonOptions):
    start_inventory_from_pool: StartInventoryPool
    goal: Goal
    missions_required: MissionsRequired
    starting_missions: StartingMissions
    starting_mission_pool: StartingMissionPool
    medal_checks: MedalChecks
    upgrade_pickups: UpgradePickups
    objective_checks: ObjectiveChecks
    upgrades_for_gold: UpgradesForGold
    death_link: DeathLink
