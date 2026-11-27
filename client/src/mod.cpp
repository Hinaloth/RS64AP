// RS64AP client: the Archipelago randomizer inside RogueSquadron64Recomp, as a native mod on the host API.
//
// Principle (docs/BIBLE.md 8.4): Archipelago owns progress, the game owns performance. Unlocks are derived from
// received items and written over the game's own values every frame; achievements are read from the game and sent
// as checks. All hook handlers run on the game thread and never block; the network lives in Net.

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "rs64/host_api.h"

#include "addresses.h"
#include "generated/rs64ap_data.h"
#include "net.h"
#include "pilot.h"
#include "rdram.h"
#include "text.h"

#ifdef _WIN32
#define MOD_EXPORT extern "C" __declspec(dllexport)
#else
#define MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using namespace rs64ap;
namespace d = rs64ap::data;
namespace a = rs64ap::addr;

// librecomp checks this on load; must be 1.
#ifdef _WIN32
extern "C" { __declspec(dllexport) uint32_t recomp_api_version = 1; }
#else
extern "C" { __attribute__((visibility("default"))) uint32_t recomp_api_version = 1; }
#endif

namespace {

constexpr const char* kModId = "rs64ap";
constexpr uint32_t kAllCraftUnlock = 0x80000u;   // gGameSettings+0xC: every craft a level allows (co-op tests use it)
constexpr int kMinHostApi = 12;

const rs64_host_api* g_api = nullptr;
Net g_net;

#if defined(__GNUC__)
void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#endif
void logf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::string line = std::string("[rs64ap] ") + buf;
    g_api->log(line.c_str());
}

uint32_t call(uint8_t* rdram, void* ctx, uint32_t vram, std::initializer_list<uint32_t> args,
              std::initializer_list<uint32_t> stack = {}) {
    uint32_t av[4] = {0, 0, 0, 0};
    size_t i = 0;
    for (uint32_t v : args) if (i < 4) av[i++] = v;
    return g_api->call(rdram, ctx, vram, av, 4, stack.size() ? stack.begin() : nullptr, (uint32_t)stack.size(),
                       0.0f, 0.0f, nullptr);
}

std::string config(const char* option) {
    const char* s = g_api->config_string(kModId, option);
    return s ? std::string(s) : std::string();
}

// ---- session state -------------------------------------------------------------------------------------------

struct Session {
    // Pilot binding
    int pilot_slot = -1;           // save_slot() last seen
    bool bound = false;            // pilot_slot has a pilot file
    PilotFile file;
    std::string file_path;
    bool dirty = false;
    int pending_bind_slot = -1;    // CONNECT pressed for this pilot
    std::string error;             // shown instead of the net status
    bool auto_connect_tried = false;

    // Derived from file.items
    bool passes[d::kMissionCount] = {};
    uint32_t ships = 0;            // bit n = craft n
    uint32_t upgrades = 0;         // bits of kUpgradeMask
    bool naboo = false;

    // From the server
    bool connected = false;
    std::set<int64_t> slot_locations;
    int goal = 0;
    int missions_required = 19;
    int medal_checks = 3;
    int ship_logic = 0;            // 0 default ship only, 1 any listed ship
    bool objective_checks = false;
    bool death_link = false;

    // Menus
    uint32_t level_mask[d::kMissionCount] = {};   // hangar mask per level, from the game (0 = unknown)
    bool masks_logged = false;
    int confirmed_level = -1;
    uint32_t status_buf = 0;
    std::string ms_shown, cs_shown;
    int medal_scan_cool = 0;
    int last_played = -1;          // owned level last flown, its medals are scanned in menus

    // Mission
    int level = -1;
    bool in_mission = false;
    bool pending_complete = false;
    uint8_t objectives_seen = 0;
    uint8_t lives_seen = 0;
    int filler_cool = 0;
    int message_cool = 0;
    int deathlink_grace = 0;
    std::deque<std::string> deathlinks;
    std::deque<std::string> mission_messages;
    std::string last_message;
} S;

bool mp_session() { return g_api->get_flag("mp_lobby_state") != 0; }
bool active() { return S.bound && S.pilot_slot >= 0 && !mp_session(); }
bool owned_level(int level) { return level >= 0 && level < d::kMissionCount && S.passes[level]; }

std::string item_name(int64_t id) {
    for (int i = 0; i < d::kMissionCount; ++i) if (d::pass_item(i) == id) return std::string(d::kMissionNames[i]) + " Pass";
    for (int i = 0; i < d::kShipCount; ++i) if (d::ship_item(i) == id) return d::kShipNames[i];
    for (int i = 0; i < d::kUpgradeCount; ++i) if (d::upgrade_item(9 + i) == id) return d::kUpgradeNames[i];
    for (const auto& f : d::kFiller) if (d::filler_item(f.index) == id) return f.name;
    return "Unknown item";
}

const d::Filler* filler_of(int64_t id) {
    for (const auto& f : d::kFiller) if (d::filler_item(f.index) == id) return &f;
    return nullptr;
}

void rebuild_state() {
    std::fill(std::begin(S.passes), std::end(S.passes), false);
    S.ships = 0;
    S.upgrades = 0;
    for (int64_t id : S.file.items) {
        for (int i = 0; i < d::kMissionCount; ++i) if (d::pass_item(i) == id) S.passes[i] = true;
        for (int i = 0; i < d::kShipCount; ++i) if (d::ship_item(i) == id) S.ships |= 1u << i;
        for (int b = 9; b < 9 + d::kUpgradeCount; ++b) if (d::upgrade_item(b) == id) S.upgrades |= 1u << b;
    }
    S.naboo = (S.ships & (1u << 8)) != 0;
}

void message(const std::string& text) {
    const std::string t = menu_text(text, 60);
    S.last_message = t;
    if (S.in_mission) S.mission_messages.push_back(t);
    logf("%s", text.c_str());
}

void save_file() {
    if (S.bound && S.dirty && !S.file_path.empty()) {
        if (!S.file.save(S.file_path)) logf("could not write %s", S.file_path.c_str());
        S.dirty = false;
    }
}

std::string pilot_path(int slot) {
    const char* dir = g_api->mod_data_dir(kModId);
    if (!dir || !*dir) return {};
    return std::string(dir) + "pilot_" + std::to_string(slot) + ".json";
}

// The pilot on the account screen changed: load its file, if it has one. A connection made for another pilot ends.
void on_pilot(int slot) {
    save_file();
    if (g_net.state() != NetState::Idle && slot != S.pending_bind_slot) {
        g_net.disconnect();
        S.connected = false;
    }
    S.pilot_slot = slot;
    S.bound = false;
    S.file = PilotFile{};
    S.file_path = slot >= 0 ? pilot_path(slot) : std::string();
    S.auto_connect_tried = false;
    if (!S.file_path.empty() && S.file.load(S.file_path)) {
        S.bound = true;
        logf("pilot %d is bound to seed %s (slot %s)", slot + 1, S.file.seed.c_str(), S.file.slot_name.c_str());
    }
    rebuild_state();
}

// ---- checks ----------------------------------------------------------------------------------------------------

void queue_location(int64_t id) {
    if (S.file.sent.count(id) || S.file.unsent.count(id)) return;
    S.file.unsent.insert(id);
    S.dirty = true;
}

void check_goal() {
    if (!S.connected || S.file.goal_sent) return;
    bool done = false;
    if (S.goal == 1) {
        int n = 0;
        for (int i = 0; i < d::kMissionCount; ++i) n += S.file.sent.count(d::location_id(i, d::kSlotComplete)) ? 1 : 0;
        done = n >= S.missions_required;
    } else {
        done = S.file.sent.count(d::location_id(15, d::kSlotComplete)) != 0;
    }
    if (done) {
        g_net.send_goal();
        S.file.goal_sent = true;
        S.dirty = true;
        message("Goal complete!");
    }
}

void flush_locations() {
    if (!S.connected || S.file.unsent.empty()) return;
    std::vector<int64_t> out;
    for (int64_t id : S.file.unsent) {
        if (S.slot_locations.count(id)) {
            out.push_back(id);
            S.file.sent.insert(id);
        }
    }
    S.file.unsent.clear();
    S.dirty = true;
    if (!out.empty()) g_net.send_locations(out);
    check_goal();
}

void scan_medals(const uint8_t* rdram, int level) {
    if (!owned_level(level)) return;
    const uint32_t acct = a::account(S.pilot_slot);
    const int medal = (rb(rdram, acct + a::kAcctMedals + (uint32_t)(level >> 2)) >> ((level & 3) * 2)) & 3;
    const int tiers = S.connected ? std::min(medal, S.medal_checks) : medal;
    for (int t = 1; t <= tiers; ++t) queue_location(d::location_id(level, t));
}

// ---- network events --------------------------------------------------------------------------------------------

void bind_or_verify(const ConnectedInfo& info) {
    const int slot = S.pending_bind_slot >= 0 ? S.pending_bind_slot : S.pilot_slot;
    S.pending_bind_slot = -1;
    if (slot < 0) {
        S.error = "CHOOSE A PILOT FIRST";
        g_net.disconnect();
        return;
    }
    if (slot != S.pilot_slot) {
        S.error = "PILOT CHANGED, CONNECT AGAIN";
        g_net.disconnect();
        return;
    }
    if (S.bound && S.file.seed != info.seed) {
        S.error = "PILOT BOUND TO ANOTHER SEED";
        g_net.disconnect();
        return;
    }
    if (!S.bound) {
        S.file = PilotFile{};
        S.file.seed = info.seed;
        S.bound = true;
        logf("pilot %d bound to seed %s", slot + 1, info.seed.c_str());
    }
    S.file.slot_name = config("slot").empty() ? S.file.slot_name : config("slot");
    S.file.server = config("server").empty() ? S.file.server : config("server");
    S.dirty = true;
}

void on_connected(ConnectedInfo info) {
    const auto& sd = info.slot_data;
    const std::string wv = sd.value("world_version", std::string("0.0.0"));
    if (wv.substr(0, wv.find('.')) != std::string(d::kWorldVersion).substr(0, std::string(d::kWorldVersion).find('.'))) {
        S.error = "APWORLD VERSION MISMATCH";
        g_net.disconnect();
        return;
    }
    S.error.clear();
    bind_or_verify(info);
    if (!S.error.empty()) return;

    S.connected = true;
    S.slot_locations = info.checked;
    S.slot_locations.insert(info.missing.begin(), info.missing.end());
    S.file.sent.insert(info.checked.begin(), info.checked.end());
    S.goal = sd.value("goal", 0);
    S.missions_required = sd.value("missions_required", 19);
    S.medal_checks = sd.value("medal_checks", 3);
    S.ship_logic = sd.value("ship_logic", 0);
    S.objective_checks = sd.value("objective_checks", 0) != 0;
    S.death_link = sd.value("death_link", 0) != 0;
    if (S.death_link) g_net.enable_deathlink_tag();
    flush_locations();
    save_file();
}

void on_items(const std::vector<ReceivedItem>& items) {
    for (const auto& it : items) {
        if (it.index >= 0 && (size_t)it.index < S.file.items.size()) continue;  // already applied (reconnect resync)
        S.file.items.push_back(it.item);
        if (filler_of(it.item)) S.file.filler_queue.push_back(it.item);
        message("Received " + item_name(it.item) + " from " + it.sender);
        S.dirty = true;
    }
    rebuild_state();
    save_file();
}

void pump() {
    if (const int slot = g_api->save_slot(); slot != S.pilot_slot) on_pilot(slot);

    ConnectedInfo info;
    if (g_net.take_connected(info)) on_connected(std::move(info));
    if (g_net.state() != NetState::SlotConnected) S.connected = false;

    if (S.connected || S.bound) {
        const auto items = g_net.take_items();
        if (!items.empty() && S.bound) on_items(items);
    }
    for (const auto& m : g_net.take_messages()) message(m);
    for (const auto& c : g_net.take_deathlinks()) {
        if (S.death_link) S.deathlinks.push_back(c);
    }
    flush_locations();
}

// ---- enforcement -----------------------------------------------------------------------------------------------

uint32_t derived_unlocks(uint32_t word) {
    return (word & ~a::kUnlockMask) | S.upgrades | kAllCraftUnlock;
}

uint32_t derived_cheats(uint32_t cheats) {
    cheats &= ~a::kLogicCheats;
    if (S.ships & (1u << 5)) cheats |= a::kCheatFarmboy;
    if (S.ships & (1u << 6)) cheats |= a::kCheatTiedup;
    if (S.passes[16]) cheats |= a::kCheatWomprat;
    if (S.passes[17]) cheats |= a::kCheatWoisthan;
    if (S.passes[18]) cheats |= a::kCheatDeaddack;
    return cheats;
}

void enforce_menus(uint8_t* rdram) {
    const uint32_t acct = a::account(S.pilot_slot);
    wb(rdram, acct + a::kAcctCurrentLevel, 15);
    ww(rdram, acct + a::kAcctUnlockWord, derived_unlocks(rw(rdram, acct + a::kAcctUnlockWord)));
    ww(rdram, a::kSettingsWord, derived_unlocks(rw(rdram, a::kSettingsWord)));
    ww(rdram, a::kActiveUnlocks, derived_unlocks(rw(rdram, a::kActiveUnlocks)));
    ww(rdram, a::kCheatFlags, derived_cheats(rw(rdram, a::kCheatFlags)));
    const uint32_t key = S.naboo ? a::kNabooKeyValue : 0u;
    ww(rdram, a::kNabooKey, key);
    ww(rdram, a::kSaveNabooKey, key);
}

void enforce_mission_upgrades(uint8_t* rdram) {
    const uint32_t word = rw(rdram, a::kSettingsWord);
    const uint32_t gained = word & a::kUpgradeMask & ~S.upgrades;
    if (gained) logf("probe pickup level=%d bits=0x%X (reverted)", S.level, gained);
    ww(rdram, a::kSettingsWord, (word & ~a::kUpgradeMask) | S.upgrades);
    ww(rdram, a::kActiveUnlocks, (rw(rdram, a::kActiveUnlocks) & ~a::kUpgradeMask) | S.upgrades);
}

// ---- menu status line (font 2, an extra slot the screen's fontAlloc hook adds) --------------------------------

void show_status(uint8_t* rdram, void* ctx, uint32_t slot, int y, const std::string& text, std::string* shown) {
    if (*shown == text) return;
    if (!S.status_buf) S.status_buf = g_api->alloc(rdram, 40);
    if (!S.status_buf) return;
    const std::string t = menu_text(text, 39);
    for (size_t i = 0; i < t.size(); ++i) wb(rdram, S.status_buf + (uint32_t)i, (uint8_t)t[i]);
    wb(rdram, S.status_buf + (uint32_t)t.size(), 0);
    call(rdram, ctx, a::kFnSetFontTextSlot, {2u, S.status_buf, slot, (uint32_t)-0x100}, {(uint32_t)y});
    call(rdram, ctx, a::kFnSetFontSlotAlpha, {2u, slot, t.empty() ? 0u : 0xFFu});
    *shown = text;
}

const char* lock_reason(int level) {
    if (level < 0 || level >= d::kMissionCount) return nullptr;
    if (!S.passes[level]) return "NO MISSION PASS";
    // The game's own hangar list for the level when known, else the data table under the seed's Ship Logic.
    const uint32_t table = d::kDefaultShip[level] | (S.ship_logic == 1 ? d::kReplayShips[level] : 0u);
    const uint32_t mask = S.level_mask[level] ? S.level_mask[level] : table;
    if (!(mask & S.ships)) return "NO SHIP FOR THIS MISSION";
    return nullptr;
}

// ---- hooks -----------------------------------------------------------------------------------------------------

int on_frame(uint32_t, uint8_t*, void*, void*) {
    pump();
    return RS64_HOOK_CONTINUE;
}

int on_menu_frame(uint32_t, uint8_t* rdram, void*, void*) {
    if (!active()) return RS64_HOOK_CONTINUE;
    enforce_menus(rdram);
    if (--S.medal_scan_cool <= 0) {
        S.medal_scan_cool = 30;
        if (S.last_played >= 0) scan_medals(rdram, S.last_played);
        save_file();
    }
    return RS64_HOOK_CONTINUE;
}

int on_mission_init(uint32_t, uint8_t* rdram, void*, void*) {
    S.level = rb(rdram, a::kCurrentLevel);
    S.in_mission = false;
    S.pending_complete = false;
    S.mission_messages.clear();
    if (active()) {
        enforce_menus(rdram);
        if (owned_level(S.level)) S.last_played = S.level;
        else logf("level %d has no Mission Pass: it sends no checks", S.level);
    }
    return RS64_HOOK_CONTINUE;
}

int on_mission_ready(uint32_t, uint8_t* rdram, void*, void*) {
    S.in_mission = true;
    S.objectives_seen = rb(rdram, a::kObjectivesDone);
    S.lives_seen = rb(rdram, a::kLives);
    S.filler_cool = 90;
    S.deathlink_grace = 0;
    logf("probe mission level=%d objectives=%u flags=0x%02X hidden=0x%02X lives=%u", S.level,
         rb(rdram, a::kObjectiveCount), S.objectives_seen, rb(rdram, a::kObjectivesHidden), S.lives_seen);
    return RS64_HOOK_CONTINUE;
}

void apply_filler(uint8_t* rdram, int64_t id) {
    const d::Filler* f = filler_of(id);
    if (!f) return;
    const std::string effect = f->effect;
    if (effect == "life") {
        const uint8_t lives = rb(rdram, a::kLives);
        if (lives < 9) wb(rdram, a::kLives, (uint8_t)(lives + 1));
        S.lives_seen = rb(rdram, a::kLives);
    } else if (effect == "secondary") {
        wb(rdram, a::kSecondary, rb(rdram, a::kSecondaryMax));
    } else if (effect == "repair") {
        wf(rdram, a::kPlayerHealth, rf(rdram, a::kPlayerHealthMax));
    }
    message(std::string(f->name) + " applied");
}

int on_mission_frame(uint32_t, uint8_t* rdram, void* ctx, void*) {
    if (!active() || S.level < 0 || S.level >= d::kMissionCount) return RS64_HOOK_CONTINUE;
    enforce_mission_upgrades(rdram);
    if (!S.in_mission) return RS64_HOOK_CONTINUE;

    const bool owned = owned_level(S.level);
    const bool calm = !g_api->in_cutscene() && rw(rdram, a::kPauseState) != 5;

    // Objectives
    const uint8_t done = rb(rdram, a::kObjectivesDone);
    if (done != S.objectives_seen) {
        const uint8_t fresh = done & ~S.objectives_seen;
        logf("probe objectives level=%d flags=0x%02X new=0x%02X", S.level, done, fresh);
        if (owned && S.objective_checks) {
            for (int b = 0; b < 8 && b < d::kMaxObjectives; ++b)
                if (fresh & (1u << b)) queue_location(d::location_id(S.level, d::kSlotObjective0 + b));
        }
        S.objectives_seen = done;
    }

    // Lives: DeathLink out
    const uint8_t lives = rb(rdram, a::kLives);
    if (lives < S.lives_seen && S.death_link && S.connected && S.deathlink_grace == 0) {
        g_net.send_deathlink(S.file.slot_name + " was shot down over " + d::kMissionNames[S.level]);
    }
    S.lives_seen = lives;
    if (S.deathlink_grace > 0) --S.deathlink_grace;

    if (!calm) return RS64_HOOK_CONTINUE;

    // DeathLink in
    if (!S.deathlinks.empty() && S.deathlink_grace == 0) {
        message("DeathLink: " + S.deathlinks.front());
        S.deathlinks.pop_front();
        wf(rdram, a::kPlayerSpawnShield, 0.0f);
        call(rdram, ctx, a::kFnApplyPlayerCraftDamage, {a::kPlayerCraft, float_bits(1.0e6f)});
        S.deathlink_grace = 150;
    }

    // Filler, one at a time
    if (S.filler_cool > 0) {
        --S.filler_cool;
    } else if (!S.file.filler_queue.empty()) {
        apply_filler(rdram, S.file.filler_queue.front());
        S.file.filler_queue.pop_front();
        S.dirty = true;
        S.filler_cool = 30;
    }

    // Messages
    if (S.message_cool > 0) {
        --S.message_cool;
    } else if (!S.mission_messages.empty()) {
        const std::string line = "message 3 " + S.mission_messages.front();
        S.mission_messages.pop_front();
        g_api->run_rule_line(rdram, ctx, line.c_str());
        S.message_cool = 90;
    }
    return RS64_HOOK_CONTINUE;
}

int on_result_success(uint32_t, uint8_t*, void*, void*) {
    if (active() && owned_level(S.level)) S.pending_complete = true;
    return RS64_HOOK_CONTINUE;
}

int on_result_fail(uint32_t, uint8_t*, void*, void*) {
    S.pending_complete = false;
    return RS64_HOOK_CONTINUE;
}

int on_mission_end(uint32_t, uint8_t*, void*, void*) {
    if (S.pending_complete && active()) {
        queue_location(d::location_id(S.level, d::kSlotComplete));
        logf("mission %d complete", S.level);
    }
    S.pending_complete = false;
    S.in_mission = false;
    flush_locations();
    save_file();
    return RS64_HOOK_CONTINUE;
}

int on_powerup_collect(uint32_t, uint8_t* rdram, void* ctx, void*) {
    const uint32_t pickup = (uint32_t)regs(ctx)->r[kS1];
    const uint32_t record = pickup ? rw(rdram, pickup + 4) : 0;
    logf("probe powerup level=%d object=0x%08X record=0x%08X", S.level, pickup, record);
    if (active() && owned_level(S.level) && d::kMissionPickup[S.level]) {
        queue_location(d::location_id(S.level, d::kSlotPickup));
    }
    return RS64_HOOK_CONTINUE;
}

int on_mission_select_init(uint32_t, uint8_t* rdram, void* ctx, void*) {
    if (!active()) return RS64_HOOK_CONTINUE;
    enforce_menus(rdram);
    std::string line = "probe craft masks consider/ignore:";
    for (int lv = 0; lv < d::kMissionCount; ++lv) {
        const uint32_t consider = call(rdram, ctx, a::kFnCraftMaskConsiderUnlocks, {(uint32_t)lv});
        const uint32_t ignore = call(rdram, ctx, a::kFnCraftMaskIgnoreUnlocks, {(uint32_t)lv});
        S.level_mask[lv] = consider;
        char part[40];
        std::snprintf(part, sizeof part, " %d=0x%X/0x%X", lv, consider, ignore);
        line += part;
    }
    if (!S.masks_logged) {
        logf("%s", line.c_str());
        S.masks_logged = true;
    }
    return RS64_HOOK_CONTINUE;
}

int on_mission_select_fonts(uint32_t, uint8_t*, void* ctx, void*) {
    if (!active()) return RS64_HOOK_CONTINUE;
    if (regs(ctx)->r[kA2] < 3) regs(ctx)->r[kA2] = 3;
    S.ms_shown = "\x01";  // force a redraw on the new screen
    return RS64_HOOK_CONTINUE;
}

int on_mission_select_tick(uint32_t, uint8_t* rdram, void* ctx, void*) {
    if (!active()) return RS64_HOOK_CONTINUE;
    const int cursor = rb(rdram, a::kMsCursor);
    const char* why = lock_reason(cursor);
    if (why) {
        const uint32_t pressed = a::kPressedWords + 4u * rb(rdram, a::kMsPort);
        ww(rdram, pressed, rw(rdram, pressed) & ~a::kButtonsConfirm);
    }
    std::string line = why ? why : (S.connected ? "" : "AP OFFLINE");
    show_status(rdram, ctx, 2, -0x7C, line, &S.ms_shown);
    return RS64_HOOK_CONTINUE;
}

int on_mission_confirmed(uint32_t, uint8_t*, void* ctx, void*) {
    S.confirmed_level = (int)(uint32_t)regs(ctx)->r[kS0];
    return RS64_HOOK_CONTINUE;
}

int on_craft_select_init(uint32_t, uint8_t*, void* ctx, void*) {
    if (!active()) return RS64_HOOK_CONTINUE;
    if (regs(ctx)->r[kA2] < 4) regs(ctx)->r[kA2] = 4;
    S.cs_shown = "\x01";
    return RS64_HOOK_CONTINUE;
}

int on_craft_select_tick(uint32_t, uint8_t* rdram, void* ctx, void*) {
    if (!active()) return RS64_HOOK_CONTINUE;
    const int level = S.confirmed_level >= 0 ? S.confirmed_level : rb(rdram, a::kCurrentLevel);
    if (level < 0 || level >= d::kMissionCount) return RS64_HOOK_CONTINUE;
    const uint32_t allowed = owned_level(level) ? (rw(rdram, a::kHangarAllowed) & S.ships) : 0u;
    ww(rdram, a::kHangarAllowed, allowed);
    if (!allowed) {
        const uint32_t pressed = a::kPressedWords + 4u * rb(rdram, a::kHangarPort);
        ww(rdram, pressed, rw(rdram, pressed) & ~a::kButtonsConfirm);
    }
    show_status(rdram, ctx, 3, -0x70, allowed ? "" : (owned_level(level) ? "NO SHIP FOR THIS MISSION" : "NO MISSION PASS"),
                &S.cs_shown);
    return RS64_HOOK_CONTINUE;
}

// ---- menu page -------------------------------------------------------------------------------------------------

ConnectConfig make_config() {
    ConnectConfig c;
    c.server = config("server");
    c.slot = config("slot");
    if (c.server.empty()) c.server = S.file.server;
    if (c.slot.empty()) c.slot = S.file.slot_name;
    c.password = config("password");
    c.cert_store = config("cert_store");
    const char* dir = g_api->mod_data_dir(kModId);
    c.uuid_file = std::string(dir ? dir : "") + "uuid";
    return c;
}

void action_connect(void*) {
    const int slot = g_api->save_slot();
    if (slot < 0) {
        S.error = "CHOOSE A PILOT FIRST";
        return;
    }
    const ConnectConfig c = make_config();
    if (c.server.empty() || c.slot.empty()) {
        S.error = "SET SERVER AND SLOT IN F6 MODS";
        return;
    }
    S.error.clear();
    S.pending_bind_slot = slot;
    g_net.connect(c);
}

void action_disconnect(void*) {
    g_net.disconnect();
    S.connected = false;
}

void action_unbind(void*) {
    if (S.pilot_slot < 0 || S.file_path.empty()) return;
    g_net.disconnect();
    std::remove(S.file_path.c_str());
    logf("pilot %d unbound", S.pilot_slot + 1);
    on_pilot(S.pilot_slot);
    S.error = "PILOT UNBOUND";
}

int condition_ready(void*) {
    // Polled while the ARCHIPELAGO page is up: a bound pilot reconnects on its own.
    if (S.bound && !S.auto_connect_tried && g_net.state() == NetState::Idle && S.error.empty()) {
        S.auto_connect_tried = true;
        const ConnectConfig c = make_config();
        if (!c.server.empty() && !c.slot.empty()) {
            S.pending_bind_slot = S.pilot_slot;
            g_net.connect(c);
        }
    }
    return S.connected && S.bound && S.pilot_slot == g_api->save_slot() ? 1 : 0;
}

const char* text_status(void*) {
    static std::string s;
    s = menu_text(S.error.empty() ? g_net.status() : S.error);
    return s.c_str();
}

const char* text_pilot(void*) {
    static std::string s;
    if (S.pilot_slot < 0) s = "NO PILOT";
    else if (!S.bound) s = "PILOT " + std::to_string(S.pilot_slot + 1) + " NOT BOUND";
    else s = menu_text("PILOT " + std::to_string(S.pilot_slot + 1) + " SLOT " + S.file.slot_name);
    return s.c_str();
}

const char* text_last(void*) {
    static std::string s;
    s = S.last_message.empty() ? std::string(" ") : menu_text(S.last_message);
    return s.c_str();
}

void on_quit(void*) {
    save_file();
    g_net.disconnect();
}

}  // namespace

MOD_EXPORT void rs64_mod_init(uint8_t* rdram, void* ctx) {
    (void)rdram;
    Regs* r = regs(ctx);
    g_api = (const rs64_host_api*)(uintptr_t)r->r[kA0];
    r->r[kV1] = RS64_HOST_API_VERSION;
    if ((uint32_t)r->r[kA1] < (uint32_t)kMinHostApi || !g_api || g_api->size < sizeof(rs64_host_api)) {
        r->r[kV0] = 1;
        return;
    }

    g_api->add_hook(RS64_HOOK_FRAME, on_frame, nullptr);
    g_api->add_hook(RS64_HOOK_MENU_FRAME, on_menu_frame, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_INIT, on_mission_init, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_READY, on_mission_ready, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_FRAME_NPCS, on_mission_frame, nullptr);
    g_api->add_hook(RS64_HOOK_RESULT_SUCCESS, on_result_success, nullptr);
    g_api->add_hook(RS64_HOOK_RESULT_FAIL, on_result_fail, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_END, on_mission_end, nullptr);
    g_api->add_hook(RS64_HOOK_POWERUP_COLLECT, on_powerup_collect, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_SELECT_INIT, on_mission_select_init, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_SELECT_FONTS, on_mission_select_fonts, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_SELECT_TICK, on_mission_select_tick, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_CONFIRMED, on_mission_confirmed, nullptr);
    g_api->add_hook(RS64_HOOK_CRAFT_SELECT_INIT, on_craft_select_init, nullptr);
    g_api->add_hook(RS64_HOOK_CRAFT_SELECT_TICK, on_craft_select_tick, nullptr);

    g_api->add_action("ap_connect", action_connect, nullptr);
    g_api->add_action("ap_disconnect", action_disconnect, nullptr);
    g_api->add_action("ap_unbind", action_unbind, nullptr);
    g_api->add_condition("ap_ready", condition_ready, nullptr);
    g_api->add_text_source("ap_status", text_status, nullptr);
    g_api->add_text_source("ap_pilot", text_pilot, nullptr);
    g_api->add_text_source("ap_last", text_last, nullptr);
    g_api->add_quit_handler(on_quit, nullptr);

    logf("host API v%u taken, world %s, game \"%s\"", (unsigned)r->r[kA1], d::kWorldVersion, d::kGame);
    r->r[kV0] = 0;
}
