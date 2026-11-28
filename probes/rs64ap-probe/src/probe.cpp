// RS64AP probe: logs game facts the randomizer's data needs, and changes nothing.
//
// Every line goes to mod_data/rs64ap-probe/probe.log (beside the game's settings) and to stderr, as
//   probe <event> key=value key=value ...
// tools/probe.py reads that log: `collect` drives the game through every mission to get objective counts, `parse`
// turns any log (also one from normal play) into a per-mission report and, with --write, into data/rs64ap.json.
//
// Events:
//   start            host API version, log path
//   mission_ready    level objectives done hidden lives          (the mission is up)
//   objective        level objectives done hidden new_done new_hidden t   (any change of the objective bytes)
//   mission_end      level result objectives done hidden          (result: 1 success, 2 failure, 3 abort/game over)
//   medal            level saved live where                       (any change, after the first look)
//   powerup          level object record                          (a pickup was collected)
//   upgrade_gained   level bits                                   (upgrade bits that appeared during the mission)
//   craft_masks      consider=... ignore=...                      (per level, once per SELECT LEVEL visit)
//   hangar           level allowed                                (craft-select allowed mask, once per visit)

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

#include "rs64/host_api.h"

#include "addresses.h"
#include "rdram.h"

#ifdef _WIN32
#define MOD_EXPORT extern "C" __declspec(dllexport)
#else
#define MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using namespace rs64ap;
namespace a = rs64ap::addr;

#ifdef _WIN32
extern "C" { __declspec(dllexport) uint32_t recomp_api_version = 1; }
#else
extern "C" { __attribute__((visibility("default"))) uint32_t recomp_api_version = 1; }
#endif

namespace {

constexpr int kLevels = 19;
const rs64_host_api* g_api = nullptr;
FILE* g_file = nullptr;

int g_level = -1;
bool g_in_mission = false;
uint8_t g_done = 0, g_hidden = 0, g_count = 0;
uint32_t g_upgrades_at_start = 0;
int g_seen_saved[kLevels], g_seen_live[kLevels];
bool g_seen_init = false;
int g_hangar_logged = -1;
int g_frame = 0;

#if defined(__GNUC__)
void probe(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#endif
void probe(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    const std::string line = std::string("[rs64ap-probe] probe ") + buf;
    g_api->log(line.c_str());
    if (g_file) {
        std::fprintf(g_file, "probe %s\n", buf);
        std::fflush(g_file);
    }
}

uint32_t call(uint8_t* rdram, void* ctx, uint32_t vram, uint32_t a0) {
    const uint32_t av[4] = {a0, 0, 0, 0};
    return g_api->call(rdram, ctx, vram, av, 4, nullptr, 0, 0.0f, 0.0f, nullptr);
}

int pilot() { return g_api->save_slot(); }

void log_medals(const uint8_t* rdram, const char* where) {
    const int slot = pilot();
    if (slot < 0) return;
    const uint32_t acct = a::account(slot);
    for (int lv = 0; lv < kLevels; ++lv) {
        const int saved = (rb(rdram, acct + a::kAcctMedals + (uint32_t)(lv >> 2)) >> ((lv & 3) * 2)) & 3;
        const int live = rb(rdram, a::kBestMedals + (uint32_t)lv);
        if (!g_seen_init) {
            g_seen_saved[lv] = saved;
            g_seen_live[lv] = live;
        } else if (saved != g_seen_saved[lv] || live != g_seen_live[lv]) {
            probe("medal level=%d saved=%d live=%d where=%s", lv, saved, live, where);
            g_seen_saved[lv] = saved;
            g_seen_live[lv] = live;
        }
    }
    g_seen_init = true;
}

int on_menu_frame(uint32_t, uint8_t* rdram, void*, void*) {
    if (++g_frame % 15 == 0) log_medals(rdram, "menu");
    return RS64_HOOK_CONTINUE;
}

int on_debrief(uint32_t, uint8_t* rdram, void*, void*) {
    log_medals(rdram, "debrief");
    return RS64_HOOK_CONTINUE;
}

int on_mission_init(uint32_t, uint8_t* rdram, void*, void*) {
    g_level = rb(rdram, a::kCurrentLevel);
    g_in_mission = false;
    return RS64_HOOK_CONTINUE;
}

int on_mission_ready(uint32_t, uint8_t* rdram, void*, void*) {
    g_in_mission = true;
    g_count = rb(rdram, a::kObjectiveCount);
    g_done = rb(rdram, a::kObjectivesDone);
    g_hidden = rb(rdram, a::kObjectivesHidden);
    g_upgrades_at_start = rw(rdram, a::kSettingsWord) & a::kUpgradeMask;
    probe("mission_ready level=%d objectives=%u done=0x%02X hidden=0x%02X lives=%u", g_level, g_count, g_done,
          g_hidden, rb(rdram, a::kLives));
    return RS64_HOOK_CONTINUE;
}

int on_mission_frame(uint32_t, uint8_t* rdram, void*, void*) {
    if (!g_in_mission) return RS64_HOOK_CONTINUE;
    const uint8_t count = rb(rdram, a::kObjectiveCount);
    const uint8_t done = rb(rdram, a::kObjectivesDone);
    const uint8_t hidden = rb(rdram, a::kObjectivesHidden);
    if (done != g_done || hidden != g_hidden || count != g_count) {
        probe("objective level=%d objectives=%u done=0x%02X hidden=0x%02X new_done=0x%02X new_hidden=0x%02X t=%.1f",
              g_level, count, done, hidden, (uint8_t)(done & ~g_done), (uint8_t)(hidden & ~g_hidden),
              g_api->mission_seconds());
        g_done = done;
        g_hidden = hidden;
        g_count = count;
    }
    const uint32_t upgrades = rw(rdram, a::kSettingsWord) & a::kUpgradeMask;
    if (upgrades & ~g_upgrades_at_start) {
        probe("upgrade_gained level=%d bits=0x%X", g_level, upgrades & ~g_upgrades_at_start);
        g_upgrades_at_start = upgrades;
    }
    return RS64_HOOK_CONTINUE;
}

int on_mission_end(uint32_t, uint8_t* rdram, void*, void*) {
    probe("mission_end level=%d result=%u objectives=%u done=0x%02X hidden=0x%02X", g_level,
          rb(rdram, a::kMissionResult), rb(rdram, a::kObjectiveCount), rb(rdram, a::kObjectivesDone),
          rb(rdram, a::kObjectivesHidden));
    g_in_mission = false;
    return RS64_HOOK_CONTINUE;
}

int on_powerup(uint32_t, uint8_t* rdram, void* ctx, void*) {
    const uint32_t object = (uint32_t)regs(ctx)->r[kS1];
    probe("powerup level=%d object=0x%08X record=0x%08X", g_level, object, object ? rw(rdram, object + 4) : 0u);
    return RS64_HOOK_CONTINUE;
}

int on_mission_select_init(uint32_t, uint8_t* rdram, void* ctx, void*) {
    std::string c, i;
    for (int lv = 0; lv < kLevels; ++lv) {
        char part[16];
        std::snprintf(part, sizeof part, "%s0x%X", lv ? "," : "", call(rdram, ctx, a::kFnCraftMaskConsiderUnlocks, (uint32_t)lv));
        c += part;
        std::snprintf(part, sizeof part, "%s0x%X", lv ? "," : "", call(rdram, ctx, a::kFnCraftMaskIgnoreUnlocks, (uint32_t)lv));
        i += part;
    }
    probe("craft_masks consider=%s ignore=%s", c.c_str(), i.c_str());
    return RS64_HOOK_CONTINUE;
}

int on_craft_select_init(uint32_t, uint8_t*, void*, void*) {
    g_hangar_logged = -1;
    return RS64_HOOK_CONTINUE;
}

int on_craft_select_tick(uint32_t, uint8_t* rdram, void*, void*) {
    const int level = rb(rdram, a::kCurrentLevel);
    if (g_hangar_logged != level) {
        probe("hangar level=%d allowed=0x%X", level, rw(rdram, a::kHangarAllowed));
        g_hangar_logged = level;
    }
    return RS64_HOOK_CONTINUE;
}

}  // namespace

MOD_EXPORT void rs64_mod_init(uint8_t*, void* ctx) {
    Regs* r = regs(ctx);
    g_api = (const rs64_host_api*)(uintptr_t)r->r[kA0];
    r->r[kV1] = RS64_HOST_API_VERSION;
    if ((uint32_t)r->r[kA1] < 12u || !g_api || g_api->size < sizeof(rs64_host_api)) {
        r->r[kV0] = 1;
        return;
    }
    const char* dir = g_api->mod_data_dir("rs64ap-probe");
    const std::string path = std::string(dir ? dir : "") + "probe.log";
    if (dir && *dir) g_file = std::fopen(path.c_str(), "a");

    g_api->add_hook(RS64_HOOK_MENU_FRAME, on_menu_frame, nullptr);
    g_api->add_hook(RS64_HOOK_DEBRIEF_PAD, on_debrief, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_INIT, on_mission_init, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_READY, on_mission_ready, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_FRAME_NPCS, on_mission_frame, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_END, on_mission_end, nullptr);
    g_api->add_hook(RS64_HOOK_POWERUP_COLLECT, on_powerup, nullptr);
    g_api->add_hook(RS64_HOOK_MISSION_SELECT_INIT, on_mission_select_init, nullptr);
    g_api->add_hook(RS64_HOOK_CRAFT_SELECT_INIT, on_craft_select_init, nullptr);
    g_api->add_hook(RS64_HOOK_CRAFT_SELECT_TICK, on_craft_select_tick, nullptr);

    probe("start host_api=%u log=%s", (unsigned)r->r[kA1], g_file ? path.c_str() : "(stderr only)");
    r->r[kV0] = 0;
}
