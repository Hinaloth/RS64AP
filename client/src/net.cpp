#include "net.h"

#include <chrono>
#include <ctime>
#include <functional>
#include <thread>
#include <utility>
#include <list>
#include <memory>

#include <apclient.hpp>
#include <apuuid.hpp>

#include "generated/rs64ap_data.h"
#include "text.h"

namespace rs64ap {

namespace {
constexpr int kItemsHandling = 0b111;  // remote items, own-world items, starting inventory
}

void Net::connect(const ConnectConfig& cfg) {
    disconnect();
    const int gen = gen_.load();
    state_ = NetState::Connecting;
    set_status(gen, "CONNECTING");
    std::thread([this, cfg, gen] { run(cfg, gen); }).detach();
}

void Net::disconnect() {
    std::lock_guard<std::mutex> lk(mu_);
    ++gen_;
    state_ = NetState::Idle;
    status_ = "NOT CONNECTED";
    items_.clear();
    messages_.clear();
    deathlinks_.clear();
    has_connected_ = false;
    out_locations_.clear();
    out_goal_ = false;
    out_deathlinks_.clear();
    out_deathlink_tag_ = false;
}

std::string Net::status() const {
    std::lock_guard<std::mutex> lk(mu_);
    return status_;
}

void Net::set_status(int gen, const std::string& s) {
    std::lock_guard<std::mutex> lk(mu_);
    if (current(gen)) status_ = menu_text(s);
}

void Net::set_state(int gen, NetState st) {
    std::lock_guard<std::mutex> lk(mu_);
    if (current(gen)) state_ = st;
}

bool Net::take_connected(ConnectedInfo& out) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!has_connected_) return false;
    out = std::move(connected_);
    has_connected_ = false;
    return true;
}

std::vector<ReceivedItem> Net::take_items() {
    std::lock_guard<std::mutex> lk(mu_);
    return std::exchange(items_, {});
}

std::vector<std::string> Net::take_messages() {
    std::lock_guard<std::mutex> lk(mu_);
    return std::exchange(messages_, {});
}

std::vector<std::string> Net::take_deathlinks() {
    std::lock_guard<std::mutex> lk(mu_);
    return std::exchange(deathlinks_, {});
}

void Net::send_locations(const std::vector<int64_t>& ids) {
    std::lock_guard<std::mutex> lk(mu_);
    out_locations_.insert(out_locations_.end(), ids.begin(), ids.end());
}

void Net::send_goal() {
    std::lock_guard<std::mutex> lk(mu_);
    out_goal_ = true;
}

void Net::send_deathlink(const std::string& cause) {
    std::lock_guard<std::mutex> lk(mu_);
    out_deathlinks_.push_back(cause);
}

void Net::enable_deathlink_tag() {
    std::lock_guard<std::mutex> lk(mu_);
    out_deathlink_tag_ = true;
}

void Net::run(ConnectConfig cfg, int gen) {
    const std::string uuid = ap_get_uuid(cfg.uuid_file);
    auto client = std::make_unique<APClient>(uuid, data::kGame, cfg.server, cfg.cert_store);
    APClient* ap = client.get();
    bool tagged = false;

    ap->set_socket_connected_handler([this, gen] { set_status(gen, "CONNECTED TO SERVER"); });
    ap->set_socket_error_handler([this, gen](const std::string& e) { set_status(gen, "ERROR " + e); });
    ap->set_socket_disconnected_handler([this, gen] {
        if (state_ == NetState::SlotConnected) {
            set_state(gen, NetState::Connecting);
            set_status(gen, "RECONNECTING");
        }
    });
    ap->set_room_info_handler([ap, &cfg, this, gen] {
        set_status(gen, "LOGGING IN");
        ap->ConnectSlot(cfg.slot, cfg.password, kItemsHandling, {}, {0, 6, 8});
    });
    ap->set_slot_refused_handler([this, gen](const std::list<std::string>& errors) {
        set_state(gen, NetState::Refused);
        set_status(gen, "REFUSED " + (errors.empty() ? std::string("") : errors.front()));
    });
    ap->set_slot_connected_handler([ap, this, gen](const nlohmann::json& slot_data) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!current(gen)) return;
        connected_ = {};
        connected_.slot_data = slot_data;
        connected_.checked = ap->get_checked_locations();
        connected_.missing = ap->get_missing_locations();
        connected_.seed = ap->get_seed();
        connected_.slot = ap->get_player_number();
        connected_.team = ap->get_team_number();
        has_connected_ = true;
        status_ = "CONNECTED";
        state_ = NetState::SlotConnected;
    });
    ap->set_items_received_handler([ap, this, gen](const std::list<APClient::NetworkItem>& list) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!current(gen)) return;
        for (const auto& it : list) {
            ReceivedItem r;
            r.index = it.index;
            r.item = it.item;
            r.location = it.location;
            r.player = it.player;
            r.sender = ap->get_player_alias(it.player);
            items_.push_back(std::move(r));
        }
    });
    // Only messages about items this slot sent or received; the rest of the multiworld's chatter stays out.
    const std::function<void(const APClient::PrintJSONArgs&)> on_print = [ap, this, gen](const APClient::PrintJSONArgs& args) {
        if (args.type != "ItemSend" || !args.item) return;
        const int me = ap->get_player_number();
        const bool mine = args.item->player == me || (args.receiving && *args.receiving == me);
        if (!mine) return;
        std::lock_guard<std::mutex> lk(mu_);
        if (current(gen)) messages_.push_back(ap->render_json(args.data, APClient::RenderFormat::TEXT));
    };
    ap->set_print_json_handler(on_print);
    ap->set_bounced_handler([ap, this, gen](const nlohmann::json& cmd) {
        const auto tags = cmd.find("tags");
        if (tags == cmd.end() || !tags->is_array()) return;
        bool death = false;
        for (const auto& t : *tags) death |= t.is_string() && t.get<std::string>() == "DeathLink";
        if (!death) return;
        const auto d = cmd.value("data", nlohmann::json::object());
        const std::string source = d.value("source", std::string("SOMEONE"));
        if (source == ap->get_slot()) return;
        std::lock_guard<std::mutex> lk(mu_);
        if (current(gen)) deathlinks_.push_back(d.value("cause", source + " died"));
    });

    while (current(gen)) {
        ap->poll();

        if (state_ == NetState::Refused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (ap->get_state() == APClient::State::SLOT_CONNECTED) {
            std::vector<int64_t> locs;
            bool goal = false, want_tag = false;
            std::vector<std::string> deaths;
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (!current(gen)) break;
                locs.swap(out_locations_);
                goal = std::exchange(out_goal_, false);
                deaths.swap(out_deathlinks_);
                want_tag = out_deathlink_tag_;
            }
            if (want_tag && !tagged) {
                ap->ConnectUpdate(false, 0, true, std::list<std::string>{"DeathLink"});
                tagged = true;
            }
            if (!locs.empty()) ap->LocationChecks(std::list<int64_t>(locs.begin(), locs.end()));
            if (goal) ap->StatusUpdate(APClient::ClientStatus::GOAL);
            for (const auto& cause : deaths) {
                const nlohmann::json body = {
                    {"time", (double)std::time(nullptr)},
                    {"cause", cause},
                    {"source", ap->get_slot()},
                };
                ap->Bounce(body, {}, {}, {"DeathLink"});
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    client.reset();
}

}  // namespace rs64ap
