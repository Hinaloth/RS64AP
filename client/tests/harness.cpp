// Loads rs64ap.so/.dll the way the game does, with a fake host API and RDRAM, connects it to a real Archipelago
// server and drives a scripted session: bind a pilot, receive the start items, check enforcement, fly one mission,
// collect its pickup, earn a medal. Exit code 0 = every check passed.
//
//   rs64ap_harness <path to rs64ap.so> <server> <slot> <mission level to fly> <data dir>
//
// tools/e2e.py generates a seed, hosts it with MultiServer.py and runs this against it.
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "rs64/host_api.h"
#include "addresses.h"
#include "generated/rs64ap_data.h"
#include "rdram.h"

using namespace rs64ap;
namespace a = rs64ap::addr;
namespace d = rs64ap::data;

namespace {

std::vector<uint8_t> g_ram(8u << 20);
uint8_t* RAM = g_ram.data();
std::map<uint32_t, std::vector<std::pair<rs64_hook_fn, void*>>> g_hooks;
std::map<std::string, std::pair<rs64_text_fn, void*>> g_texts;
std::map<std::string, std::pair<rs64_action_fn, void*>> g_actions;
std::map<std::string, std::pair<rs64_condition_fn, void*>> g_conditions;
std::map<std::string, int> g_flags;
std::map<std::string, std::string> g_config;
std::vector<std::string> g_rule_lines;
std::string g_data_dir;
int g_save_slot = -1;
uint32_t g_alloc = 0x80700000u;
int g_failures = 0;
uint32_t g_craft_mask = 0x7Fu;
std::vector<uint32_t> g_damage_calls;

void hlog(const char* line) { std::fprintf(stderr, "%s\n", line); }
int add_hook(uint32_t hook, rs64_hook_fn fn, void* user) { g_hooks[hook].push_back({fn, user}); return 0; }
int add_text(const char* n, rs64_text_fn fn, void* u) { g_texts[n] = {fn, u}; return 0; }
int add_input(rs64_input_fn, void*) { return 0; }
void set_flag(const char* n, int v) { g_flags[n] = v; }
int get_flag(const char* n) { auto it = g_flags.find(n); return it == g_flags.end() ? 0 : it->second; }
uint32_t call(uint8_t*, void*, uint32_t vram, const uint32_t* args, uint32_t, const uint32_t*, uint32_t, float, float, float*) {
    if (vram == a::kFnCraftMaskConsiderUnlocks || vram == a::kFnCraftMaskIgnoreUnlocks) return g_craft_mask;
    if (vram == a::kFnApplyPlayerCraftDamage) g_damage_calls.push_back(args[1]);
    return 0;
}
uint32_t alloc(uint8_t*, uint32_t size) { const uint32_t r = g_alloc; g_alloc += (size + 15u) & ~15u; return r; }
const char* state_id() { return "menu.level_select"; }
int in_cutscene() { return 0; }
int add_action(const char* n, rs64_action_fn fn, void* u) { g_actions[n] = {fn, u}; return 0; }
int add_condition(const char* n, rs64_condition_fn fn, void* u) { g_conditions[n] = {fn, u}; return 0; }
int add_quit(rs64_event_fn, void*) { return 0; }
int run_rule_line(uint8_t*, void*, const char* line) { g_rule_lines.push_back(line); return 0; }
const char* config_string(const char*, const char* opt) {
    static std::string s;
    auto it = g_config.find(opt);
    s = it == g_config.end() ? std::string() : it->second;
    return s.c_str();
}
const char* mod_data_dir(const char*) { return g_data_dir.c_str(); }
int save_slot() { return g_save_slot; }
float g_mission_time = 0.0f;
float mission_seconds() { return g_mission_time; }

void fire(uint32_t hook, Regs* regs = nullptr) {
    Regs local{};
    for (auto& [fn, user] : g_hooks[hook]) fn(hook, RAM, regs ? regs : &local, user);
}

void frames(int n, uint32_t hook = RS64_HOOK_MENU_FRAME) {
    for (int i = 0; i < n; ++i) {
        fire(RS64_HOOK_FRAME);
        fire(hook);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string text(const char* name) {
    auto it = g_texts.find(name);
    return it == g_texts.end() ? std::string("?") : std::string(it->second.first(it->second.second));
}

void expect(bool ok, const char* what, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, what);
    std::vsnprintf(buf, sizeof buf, what, ap);
    va_end(ap);
    std::fprintf(stderr, "%s %s\n", ok ? "[PASS]" : "[FAIL]", buf);
    if (!ok) ++g_failures;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool wait_ready(int seconds) {
    for (int i = 0; i < seconds * 50; ++i) {
        frames(1);
        auto& c = g_conditions["ap_ready"];
        if (c.first && c.first(c.second)) return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: %s <rs64ap library> <server> <slot> <level> <data dir>\n", argv[0]);
        return 2;
    }
    const int level = std::atoi(argv[4]);
    g_data_dir = std::string(argv[5]) + "/";
    g_config["server"] = argv[2];
    g_config["slot"] = argv[3];

#ifdef _WIN32
    HMODULE lib = LoadLibraryA(argv[1]);
    auto init = lib ? (void (*)(uint8_t*, void*))GetProcAddress(lib, "rs64_mod_init") : nullptr;
#else
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) std::fprintf(stderr, "%s\n", dlerror());
    auto init = lib ? (void (*)(uint8_t*, void*))dlsym(lib, "rs64_mod_init") : nullptr;
#endif
    if (!init) return 2;

    static rs64_host_api api{};
    api.version = RS64_HOST_API_VERSION;
    api.size = sizeof(rs64_host_api);
    api.log = hlog;
    api.add_hook = add_hook;
    api.add_text_source = add_text;
    api.add_input_filter = add_input;
    api.set_flag = set_flag;
    api.get_flag = get_flag;
    api.call = call;
    api.alloc = alloc;
    api.state_id = state_id;
    api.in_cutscene = in_cutscene;
    api.add_action = add_action;
    api.add_condition = add_condition;
    api.add_quit_handler = add_quit;
    api.run_rule_line = run_rule_line;
    api.config_string = config_string;
    api.mod_data_dir = mod_data_dir;
    api.save_slot = save_slot;
    api.mission_seconds = mission_seconds;

    Regs r{};
    r.r[kA0] = (uint64_t)(uintptr_t)&api;
    r.r[kA1] = RS64_HOST_API_VERSION;
    init(RAM, &r);
    expect(r.r[kV0] == 0, "mod init accepted (r2=%llu)", (unsigned long long)r.r[kV0]);

    // A non-AP pilot's unlocks are left alone.
    g_save_slot = 1;
    ww(RAM, a::account(1) + a::kAcctUnlockWord, 0x1FE00u);
    ww(RAM, a::kCheatFlags, a::kCheatFarmboy);
    frames(5);
    expect(rw(RAM, a::account(1) + a::kAcctUnlockWord) == 0x1FE00u, "unbound pilot untouched");

    // Bind pilot 1 (slot 0) through the ARCHIPELAGO page.
    g_save_slot = 0;
    frames(2);
    g_actions["ap_connect"].first(nullptr);
    const bool ready = wait_ready(20);
    expect(ready, "connected and bound (status: %s)", text("ap_status").c_str());
    if (!ready) return 1;
    frames(20);
    expect(text("ap_pilot").find("SLOT") != std::string::npos, "pilot label: %s", text("ap_pilot").c_str());

    // Enforcement in menus.
    const uint32_t acct = a::account(0);
    expect(rb(RAM, acct + a::kAcctCurrentLevel) == 15, "pilot current_level forced to 15");
    expect((rw(RAM, a::kCheatFlags) & a::kCheatFarmboy) == 0, "FARMBOY cleared (no Falcon received)");
    expect((rw(RAM, a::kSettingsWord) & a::kUpgradeMask) == 0, "no upgrades before any are received");
    expect((rw(RAM, a::kSettingsWord) & 0x80000u) != 0, "all-craft unlock bit set");

    // Mission select: the flown mission is owned, mission 0 is locked unless it was a starting mission.
    Regs ms{};
    fire(RS64_HOOK_MISSION_SELECT_INIT, &ms);
    wb(RAM, a::kMsCursor, (uint8_t)level);
    ww(RAM, a::kPressedWords, 0x8000u);
    fire(RS64_HOOK_MISSION_SELECT_TICK, &ms);
    expect((rw(RAM, a::kPressedWords) & 0x8000u) != 0, "A passes on owned mission %d", level);
    const int locked = level == 15 ? 16 : 15;  // Calamari is never a starting mission with goal calamari
    wb(RAM, a::kMsCursor, (uint8_t)locked);
    ww(RAM, a::kPressedWords, 0x8000u);
    fire(RS64_HOOK_MISSION_SELECT_TICK, &ms);
    expect((rw(RAM, a::kPressedWords) & 0x8000u) == 0, "A blocked on locked mission %d", locked);

    // Hangar: only owned ships stay in the allowed mask.
    Regs confirm{};
    confirm.r[kS0] = (uint64_t)level;
    fire(RS64_HOOK_MISSION_CONFIRMED, &confirm);
    ww(RAM, a::kHangarAllowed, 0x7Fu);
    fire(RS64_HOOK_CRAFT_SELECT_TICK, &ms);
    const uint32_t hangar = rw(RAM, a::kHangarAllowed);
    expect(hangar != 0 && (hangar & ~0x7Fu) == 0, "hangar mask narrowed to owned ships (0x%X)", hangar);

    // Fly the mission: a pickup, a stray upgrade bit that must be reverted, success.
    wb(RAM, a::kCurrentLevel, (uint8_t)level);
    wb(RAM, a::kLives, 3);
    wb(RAM, a::kSecondary, 0);
    wb(RAM, a::kSecondaryMax, 4);
    g_mission_time = 0.0f;
    fire(RS64_HOOK_MISSION_INIT);
    fire(RS64_HOOK_MISSION_READY);
    frames(3, RS64_HOOK_MISSION_FRAME_NPCS);

    // Replenishing Secondaries (start inventory): +1 after 15 s, none before.
    g_mission_time = 14.0f;
    frames(2, RS64_HOOK_MISSION_FRAME_NPCS);
    expect(rb(RAM, a::kSecondary) == 0, "no secondary regen before 15 s");
    g_mission_time = 15.5f;
    frames(2, RS64_HOOK_MISSION_FRAME_NPCS);
    expect(rb(RAM, a::kSecondary) == 1, "secondary regen +1 at 15 s (count %u)", rb(RAM, a::kSecondary));
    wb(RAM, a::kSecondary, 0xFF);
    g_mission_time = 31.0f;
    frames(2, RS64_HOOK_MISSION_FRAME_NPCS);
    expect(rb(RAM, a::kSecondary) == 0xFF, "unlimited (0xFF) secondaries left alone");

    // Temporary Invincibility (start inventory, filler): applied from the queue, then damage is zeroed for 15 s.
    auto damage = [&](float dmg) {
        Regs dr{};
        dr.r[kA0] = a::kPlayerCraft;
        dr.r[kA1] = float_bits(dmg);
        fire(RS64_HOOK_PLAYER_DAMAGE, &dr);
        return (uint32_t)dr.r[kA1];
    };
    expect(damage(5.0f) == float_bits(5.0f), "damage passes before invincibility");
    g_mission_time = 40.0f;
    bool invincible = false;
    for (int i = 0; i < 400 && !invincible; ++i) {
        frames(1, RS64_HOOK_MISSION_FRAME_NPCS);
        invincible = damage(5.0f) == 0;
    }
    expect(invincible, "Temporary Invincibility zeroes player damage");
    g_mission_time = 54.0f;
    frames(2, RS64_HOOK_MISSION_FRAME_NPCS);
    expect(damage(5.0f) == 0, "still invincible at 14 s");
    g_mission_time = 56.0f;
    frames(2, RS64_HOOK_MISSION_FRAME_NPCS);
    expect(damage(5.0f) == float_bits(5.0f), "invincibility over after 15 s");
    ww(RAM, a::kSettingsWord, rw(RAM, a::kSettingsWord) | 0x10000u);  // the pickup's own grant
    Regs pick{};
    pick.r[kS1] = 0x80600000u;
    fire(RS64_HOOK_POWERUP_COLLECT, &pick);
    frames(2, RS64_HOOK_MISSION_FRAME_NPCS);
    expect((rw(RAM, a::kSettingsWord) & 0x10000u) == 0, "picked-up upgrade bit reverted");
    // Medal Leniency (start inventory): a fake medal table the debrief would read.
    const uint32_t table = 0x80650000u;
    for (uint32_t lv = 0; lv < 19; ++lv) {
        for (uint32_t t = 0; t < 3; ++t) {
            const uint32_t rec = table + lv * 0x24u + t * 0xCu;
            wf(RAM, rec, 10.0f);
            const uint16_t v[4] = {30, 60, 9, 1};
            for (int k = 0; k < 4; ++k) { wb(RAM, rec + 4 + 2 * k, (uint8_t)(v[k] >> 8)); wb(RAM, rec + 5 + 2 * k, (uint8_t)v[k]); }
        }
    }
    ww(RAM, a::kMedalInfoPtr, table);

    fire(RS64_HOOK_RESULT_SUCCESS);
    fire(RS64_HOOK_MISSION_END);
    frames(1);
    const uint32_t eased = rw(RAM, a::kMedalInfoPtr);
    expect(eased != table, "debrief medal table swapped for the leniency copy");
    {
        const uint32_t rec = eased + (uint32_t)level * 0x24u + 0xCu;  // silver
        auto u16 = [&](uint32_t at) { return (rb(RAM, at) << 8) | rb(RAM, at + 1); };
        expect(rf(RAM, rec) == 15.0f && u16(rec + 4) == 20 && u16(rec + 6) == 40 && u16(rec + 8) == 6 && u16(rec + 10) == 1,
               "silver eased: %.1f min, kills %d, acc %d, friendlies %d, bonus %d", rf(RAM, rec), u16(rec + 4),
               u16(rec + 6), u16(rec + 8), u16(rec + 10));
        const uint32_t other = eased + (uint32_t)((level + 1) % 19) * 0x24u;
        expect(rf(RAM, other) == 10.0f && u16(other + 4) == 30, "other levels untouched");
    }
    Regs back{};
    fire(RS64_HOOK_MISSION_SELECT_INIT, &back);
    expect(rw(RAM, a::kMedalInfoPtr) == table, "medal table pointer restored at mission select");

    // Debrief writes a silver medal for the level.
    const uint32_t medal_byte = acct + a::kAcctMedals + (uint32_t)(level >> 2);
    wb(RAM, medal_byte, (uint8_t)(rb(RAM, medal_byte) | (2u << ((level & 3) * 2))));
    frames(100);

    const std::string pilot = slurp(g_data_dir + "pilot_0.json");
    auto has = [&](int64_t id) { return pilot.find(std::to_string(id)) != std::string::npos; };
    expect(has(d::location_id(level, d::kSlotComplete)), "complete check recorded");
    expect(has(d::location_id(level, d::kSlotBronze)), "bronze check recorded");
    expect(has(d::location_id(level, d::kSlotSilver)), "silver check recorded");
    expect(!d::kMissionPickup[level] || has(d::location_id(level, d::kSlotPickup)), "pickup check recorded");
    expect(pilot.find("\"unsent\": []") != std::string::npos, "nothing left unsent");
    std::fprintf(stderr, "last message: %s\n", text("ap_last").c_str());

    // Reconnect: the item list must not grow (resync is skipped).
    const size_t items_before = std::count(pilot.begin(), pilot.end(), ',');
    g_actions["ap_disconnect"].first(nullptr);
    frames(5);
    g_actions["ap_connect"].first(nullptr);
    expect(wait_ready(20), "reconnected");
    frames(50);
    const std::string pilot2 = slurp(g_data_dir + "pilot_0.json");
    expect(std::count(pilot2.begin(), pilot2.end(), ',') == (long)items_before, "reconnect did not duplicate items");

    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    g_actions["ap_disconnect"].first(nullptr);
    std::fflush(stderr);
    std::_Exit(g_failures ? 1 : 0);
}
