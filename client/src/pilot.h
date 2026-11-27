// Per-pilot Archipelago state, kept in mod_data/rs64ap/pilot_<slot>.json beside the game's settings.
// A pilot is bound to one seed; everything the client needs to enforce unlocks offline is here.
#pragma once
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

namespace rs64ap {

struct PilotFile {
    std::string seed;
    std::string slot_name;
    std::string server;
    std::vector<int64_t> items;        // every item received, in server index order
    std::set<int64_t> sent;            // locations the server has (or was sent)
    std::set<int64_t> unsent;          // detected while offline, sent on the next connect
    std::deque<int64_t> filler_queue;  // filler waiting for an active mission
    bool goal_sent = false;

    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace rs64ap
