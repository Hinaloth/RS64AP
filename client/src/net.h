// The Archipelago connection. apclientpp lives on its own thread; the game thread only talks to it through the
// queues here, so no hook ever blocks on the network.
#pragma once
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace rs64ap {

struct ConnectConfig {
    std::string server;
    std::string slot;
    std::string password;
    std::string cert_store;   // optional CA bundle path for wss
    std::string uuid_file;
};

enum class NetState { Idle, Connecting, SlotConnected, Refused, Failed };

struct ConnectedInfo {
    nlohmann::json slot_data;
    std::set<int64_t> checked;
    std::set<int64_t> missing;
    std::string seed;
    int slot = 0;
    int team = 0;
};

struct ReceivedItem {
    int index = -1;
    int64_t item = 0;
    int64_t location = 0;
    int player = 0;
    std::string sender;
};

class Net {
public:
    // Game thread.
    void connect(const ConnectConfig& cfg);
    void disconnect();
    NetState state() const { return state_.load(); }
    std::string status() const;

    bool take_connected(ConnectedInfo& out);
    std::vector<ReceivedItem> take_items();
    std::vector<std::string> take_messages();
    std::vector<std::string> take_deathlinks();   // causes

    void send_locations(const std::vector<int64_t>& ids);
    void send_goal();
    void send_deathlink(const std::string& cause);
    void enable_deathlink_tag();

private:
    // Each connect() starts a detached worker tagged with a generation; disconnect() bumps the generation, so the
    // game thread never waits for a worker to finish. A stale worker drops everything it would report and exits.
    void run(ConnectConfig cfg, int gen);
    bool current(int gen) const { return gen == gen_.load(); }
    void set_status(int gen, const std::string& s);
    void set_state(int gen, NetState st);

    std::atomic<int> gen_{0};
    std::atomic<NetState> state_{NetState::Idle};

    mutable std::mutex mu_;
    std::string status_ = "NOT CONNECTED";
    bool has_connected_ = false;
    ConnectedInfo connected_;
    std::vector<ReceivedItem> items_;
    std::vector<std::string> messages_;
    std::vector<std::string> deathlinks_;

    std::vector<int64_t> out_locations_;
    bool out_goal_ = false;
    std::vector<std::string> out_deathlinks_;
    bool out_deathlink_tag_ = false;
};

}  // namespace rs64ap
