#include "epocx_wire_replay.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {

constexpr const char* kMagic = "EPOCXWIRE/1";

double steady_now() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

} // namespace

EpocXWireReplay::EpocXWireReplay(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open wire replay file: " + path);
    }

    std::vector<std::string> header_lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() && !header_lines.empty()) {
            break;
        }
        header_lines.push_back(line);
    }

    if (header_lines.empty() || header_lines[0] != kMagic) {
        throw std::runtime_error(std::string("Bad epocxwire magic; expected ") + kMagic);
    }
    for (size_t i = 1; i < header_lines.size(); ++i) {
        const auto& hl = header_lines[i];
        const auto eq = hl.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = hl.substr(0, eq);
        const std::string val = hl.substr(eq + 1);
        if (key == "serial") {
            header_.serial = val;
        } else if (key == "serial_source") {
            header_.serial_source = val;
        } else if (key == "adc_div") {
            header_.adc_div = std::stod(val);
        } else if (key == "eeg_hz") {
            header_.eeg_hz = std::stod(val);
        }
    }

    while (in) {
        uint64_t rel_ns = 0;
        uint16_t nbytes = 0;
        in.read(reinterpret_cast<char*>(&rel_ns), sizeof(rel_ns));
        if (!in) {
            break;
        }
        in.read(reinterpret_cast<char*>(&nbytes), sizeof(nbytes));
        if (!in) {
            throw std::runtime_error("Truncated packet header in wire file");
        }
        EpocXWirePacket pkt;
        pkt.rel_ns = rel_ns;
        pkt.payload.resize(nbytes);
        if (nbytes > 0) {
            in.read(reinterpret_cast<char*>(pkt.payload.data()), nbytes);
            if (!in) {
                throw std::runtime_error("Truncated packet payload in wire file");
            }
        }
        packets_.push_back(std::move(pkt));
    }
}

void EpocXWireReplay::reset() {
    index_ = 0;
    wall_started_ = false;
}

std::vector<uint8_t> EpocXWireReplay::read(double speed) {
    if (index_ >= packets_.size()) {
        return {};
    }
    const EpocXWirePacket& pkt = packets_[index_++];
    if (!wall_started_) {
        wall_t0_ = steady_now();
        wall_started_ = true;
    }
    if (speed > 0.0) {
        const double target = wall_t0_ + (static_cast<double>(pkt.rel_ns) * 1e-9) / speed;
        const double delay = target - steady_now();
        if (delay > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        }
    }
    return pkt.payload;
}
