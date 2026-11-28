#include "pilot.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

namespace rs64ap {

bool PilotFile::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    try {
        const nlohmann::json j = nlohmann::json::parse(in);
        seed = j.value("seed", std::string());
        slot_name = j.value("slot_name", std::string());
        server = j.value("server", std::string());
        pilot_id = j.value("pilot_id", -1);
        pilot_name = j.value("pilot_name", std::string());
        items = j.value("items", std::vector<int64_t>());
        const auto s = j.value("sent", std::vector<int64_t>());
        sent = std::set<int64_t>(s.begin(), s.end());
        const auto u = j.value("unsent", std::vector<int64_t>());
        unsent = std::set<int64_t>(u.begin(), u.end());
        const auto f = j.value("filler_queue", std::vector<int64_t>());
        filler_queue = std::deque<int64_t>(f.begin(), f.end());
        goal_sent = j.value("goal_sent", false);
        return !seed.empty();
    } catch (const std::exception&) {
        return false;
    }
}

bool PilotFile::save(const std::string& path) const {
    const nlohmann::json j = {
        {"seed", seed},
        {"slot_name", slot_name},
        {"server", server},
        {"pilot_id", pilot_id},
        {"pilot_name", pilot_name},
        {"items", items},
        {"sent", std::vector<int64_t>(sent.begin(), sent.end())},
        {"unsent", std::vector<int64_t>(unsent.begin(), unsent.end())},
        {"filler_queue", std::vector<int64_t>(filler_queue.begin(), filler_queue.end())},
        {"goal_sent", goal_sent},
    };
    // Write to a temp file, then replace, so a crash mid-write never leaves a broken pilot file.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << j.dump(1);
        if (!out) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace rs64ap
