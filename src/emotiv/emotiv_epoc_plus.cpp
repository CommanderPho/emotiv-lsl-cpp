#include "emotiv_epoc_plus.h"
#include "config.h"
#include <iostream>

EmotivEpocPlus::EmotivEpocPlus(bool enable_quality, bool fourteen_bit, const std::string& record_file)
    : EmotivBase(false, enable_quality, record_file), is_fourteen_bit_mode(fourteen_bit) {
    device_name = "Emotiv Epoc+";
    KeyModel = 6;
    READ_SIZE = 32;
}

EmotivEpocPlus::~EmotivEpocPlus() {}

std::vector<uint8_t> EmotivEpocPlus::get_crypto_key() {
    if (serial_number.empty()) {
        hid_device* dev = get_hid_device();
        if (dev) hid_close(dev);
    }
    std::string serial = serial_number;
    std::vector<uint8_t> key;
    if (serial.length() >= 16) {
        int len = serial.length();
        if (!is_fourteen_bit_mode) {
            key.push_back(serial[len - 1]);
            key.push_back(serial[len - 2]);
            key.push_back(serial[len - 2]);
            key.push_back(serial[len - 3]);
            key.push_back(serial[len - 3]);
            key.push_back(serial[len - 3]);
            key.push_back(serial[len - 2]);
            key.push_back(serial[len - 4]);
            key.push_back(serial[len - 1]);
            key.push_back(serial[len - 4]);
            key.push_back(serial[len - 2]);
            key.push_back(serial[len - 2]);
            key.push_back(serial[len - 4]);
            key.push_back(serial[len - 4]);
            key.push_back(serial[len - 2]);
            key.push_back(serial[len - 1]);
        } else {
            key.push_back(serial[len - 1]);
            key.push_back(0);
            key.push_back(serial[len - 2]);
            key.push_back(21);
            key.push_back(serial[len - 3]);
            key.push_back(0);
            key.push_back(serial[len - 4]);
            key.push_back(12);
            key.push_back(serial[len - 3]);
            key.push_back(0);
            key.push_back(serial[len - 2]);
            key.push_back(68);
            key.push_back(serial[len - 1]);
            key.push_back(0);
            key.push_back(serial[len - 2]);
            key.push_back(88);
        }
    } else {
        throw std::runtime_error("Serial number too short to derive key for Epoc+");
    }
    return key;
}

EmotivData EmotivEpocPlus::decode_data(const std::vector<uint8_t>& data) {
    if (!ctx_initialized) {
        std::vector<uint8_t> key = get_crypto_key();
        AES_init_ctx(&ctx, key.data());
        ctx_initialized = true;
    }

    std::vector<uint8_t> dec_data = data;
    // Epoc+ does NOT XOR with 0x55
    AES_ECB_decrypt(&ctx, dec_data.data());
    AES_ECB_decrypt(&ctx, dec_data.data() + 16);

    EmotivData result;

    if (dec_data.size() > 1 && dec_data[1] == 32) {
        // Motion packet (gyro) not yet supported for Epoc+ in decode_data
        result.has_motion = false;
        return result;
    }

    if (enable_electrode_quality_stream) {
        try {
            result.quality_data = extractQualityValues(dec_data);
            result.has_quality = true;
        } catch (...) {
            // failed
        }
    }

    std::vector<double> packet_data;
    for (int i = 2; i < 16; i += 2) {
        packet_data.push_back(convertEPOC_PLUS(dec_data[i], dec_data[i+1]));
    }
    for (size_t i = 18; i < dec_data.size(); i += 2) {
        packet_data.push_back(convertEPOC_PLUS(dec_data[i], dec_data[i+1]));
    }

    if (packet_data.size() == 14) {
        // Swap positions matching Python logic
        std::swap(packet_data[0], packet_data[2]);   // AF3 and F3
        std::swap(packet_data[13], packet_data[11]); // AF4 and F4
        std::swap(packet_data[1], packet_data[3]);   // F7 and FC5
        std::swap(packet_data[10], packet_data[12]); // FC6 and F8
        result.eeg_data = packet_data;
        result.has_eeg = true;
    }

    return result;
}

bool EmotivEpocPlus::validate_data(const std::vector<uint8_t>& data) {
    return data.size() == static_cast<size_t>(READ_SIZE);
}
