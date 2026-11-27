from BaseClasses import Location

from . import data

MEDAL_TIERS: dict[int, str] = {1: "Bronze Medal", 2: "Silver Medal", 3: "Gold Medal"}

LOCATION_GROUPS: dict[str, set[str]] = {
    "Mission Completions": {f"{m['name']} - Complete" for m in data.MISSIONS},
    "Medals": {f"{m['name']} - {t}" for m in data.MISSIONS for t in MEDAL_TIERS.values()},
    "Upgrade Pickups": {f"{m['name']} - Upgrade Pickup" for m in data.MISSIONS if m["pickup"]},
}


class RS64Location(Location):
    game = data.GAME


def complete(mission: dict) -> str:
    return f"{mission['name']} - Complete"


def medal(mission: dict, tier: int) -> str:
    return f"{mission['name']} - {MEDAL_TIERS[tier]}"


def pickup(mission: dict) -> str:
    return f"{mission['name']} - Upgrade Pickup"


def objective(mission: dict, n: int) -> str:
    return f"{mission['name']} - Objective {n}"
