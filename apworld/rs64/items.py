from BaseClasses import Item, ItemClassification

from . import data

MISSION_PASSES: list[str] = [f"{m['name']} Pass" for m in data.MISSIONS]
CAMPAIGN_PASSES: list[str] = [f"{m['name']} Pass" for m in data.MISSIONS if m["kind"] != "bonus"]
BONUS_PASSES: list[str] = [f"{m['name']} Pass" for m in data.MISSIONS if m["kind"] == "bonus"]
SHIPS: list[str] = [s["name"] for s in data.SHIPS]
UPGRADES: list[str] = [u["name"] for u in data.UPGRADES]
FILLER: list[str] = [f["name"] for f in data.FILLER]
FILLER_WEIGHTS: list[int] = [f["weight"] for f in data.FILLER]

ITEM_GROUPS: dict[str, set[str]] = {
    "Mission Passes": set(MISSION_PASSES),
    "Campaign Passes": set(CAMPAIGN_PASSES),
    "Bonus Passes": set(BONUS_PASSES),
    "Ships": set(SHIPS),
    "Upgrades": set(UPGRADES),
}

# Generation-only events, no IDs.
VICTORY = "Victory"
MISSION_CLEARED = "Mission Cleared"


class RS64Item(Item):
    game = data.GAME


def base_classification(name: str) -> ItemClassification:
    if name in MISSION_PASSES or name in SHIPS:
        return ItemClassification.progression
    if name in UPGRADES:
        return ItemClassification.useful
    return ItemClassification.filler
