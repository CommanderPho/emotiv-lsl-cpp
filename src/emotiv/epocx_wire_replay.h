#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct EpocXWireHeader {
    std::string serial = "UD20221202006756";
    std::string serial_source = "hid";
    double adc_div = 7.8;
    double eeg_hz = 128.0;
};

struct EpocXWirePacket {
    uint64_t rel_ns = 0;
    std::vector<uint8_t> payload;
};

/** Reader for `.epocxwire` dumps (see emotiv-lsl emotiv_lsl/epocx_wire.py). */
class EpocXWireReplay {
public:
    explicit EpocXWireReplay(const std::string& path);

    const EpocXWireHeader& header() const { return header_; }
    const std::vector<EpocXWirePacket>& packets() const { return packets_; }
    size_t size() const { return packets_.size(); }
    bool eof() const { return index_ >= packets_.size(); }

    /** Return next payload; empty vector on EOF. Honors speed (0 = ASAP). */
    std::vector<uint8_t> read(double speed);

    void reset();

private:
    EpocXWireHeader header_;
    std::vector<EpocXWirePacket> packets_;
    size_t index_ = 0;
    bool wall_started_ = false;
    double wall_t0_ = 0.0;
};
