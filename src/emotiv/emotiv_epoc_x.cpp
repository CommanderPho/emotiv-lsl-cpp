#include "emotiv_epoc_x.h"
#include "config.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

int16_t read_be_i16(const uint8_t* p) {
    return static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
}

uint16_t read_le_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

} // namespace

std::array<uint8_t, 4> serial_bytes_hid(const std::string& serial) {
    if (serial.size() < 4) {
        throw std::runtime_error("HID serial must have length >= 4");
    }
    const size_t n = serial.size();
    return {
        static_cast<uint8_t>(serial[n - 1]),
        static_cast<uint8_t>(serial[n - 2]),
        static_cast<uint8_t>(serial[n - 3]),
        static_cast<uint8_t>(serial[n - 4]),
    };
}

std::array<uint8_t, 4> serial_bytes_ble_hex(const std::string& serial_hex) {
    std::string cleaned;
    cleaned.reserve(serial_hex.size());
    for (char c : serial_hex) {
        if (c == ' ' || c == '-' || c == ':') {
            continue;
        }
        cleaned.push_back(c);
    }
    if (cleaned.size() >= 2 && cleaned[0] == '0' && (cleaned[1] == 'x' || cleaned[1] == 'X')) {
        cleaned = cleaned.substr(2);
    }
    if (cleaned.size() < 8 || (cleaned.size() % 2) != 0) {
        throw std::runtime_error("BLE hex serial needs >= 4 bytes");
    }
    auto hex_nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return 10 + (c - 'a');
        }
        if (c >= 'A' && c <= 'F') {
            return 10 + (c - 'A');
        }
        throw std::runtime_error("Invalid hex digit in BLE serial");
    };
    std::array<uint8_t, 4> out{};
    for (size_t i = 0; i < 4; ++i) {
        out[i] = static_cast<uint8_t>((hex_nibble(cleaned[2 * i]) << 4) | hex_nibble(cleaned[2 * i + 1]));
    }
    return out;
}

std::array<uint8_t, 16> aes_key_epocx_mode(const std::array<uint8_t, 4>& s4) {
    std::array<uint8_t, 16> key{};
    for (size_t i = 0; i < kEpocXModePerm.size(); ++i) {
        key[i] = s4[static_cast<size_t>(kEpocXModePerm[i])];
    }
    return key;
}

std::vector<uint8_t> decrypt_eeg_packet(const std::vector<uint8_t>& data, const uint8_t* key16) {
    if (data.size() < 32) {
        throw std::runtime_error("EEG packet needs 32 bytes");
    }
    std::vector<uint8_t> plain(data.begin(), data.begin() + 32);
    for (size_t i = 0; i < 32; ++i) {
        plain[i] ^= 0x55;
    }
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key16);
    AES_ECB_decrypt(&ctx, plain.data());
    AES_ECB_decrypt(&ctx, plain.data() + 16);
    return plain;
}

EmotivData decode_eeg_mode7(const std::vector<uint8_t>& plain, bool enable_quality, double adc_div) {
    EmotivData result;
    if (plain.size() < 32) {
        return result;
    }
    const uint8_t pkg_type = plain[1] & 0x7f;
    if (pkg_type != kEegType) {
        return result;
    }

    uint16_t words[15];
    for (int i = 0; i < 15; ++i) {
        words[i] = read_le_u16(plain.data() + 2 + 2 * i);
    }

    result.eeg_data.reserve(kEpocXRawEegIdMode7.size());
    for (int idx : kEpocXRawEegIdMode7) {
        result.eeg_data.push_back(static_cast<double>(words[idx]) / adc_div);
    }
    result.has_eeg = true;

    if (enable_quality) {
        const double cq = static_cast<double>(words[7] & 0x7f);
        result.quality_data.assign(14, cq);
        result.has_quality = true;
    }
    return result;
}

std::vector<double> decode_motion_20(const std::vector<uint8_t>& data) {
    if (data.size() < 20) {
        return {};
    }

    int16_t raw[9];
    for (int i = 0; i < 9; ++i) {
        raw[i] = read_be_i16(data.data() + 2 + 2 * i);
    }

    // correctMotionDirection (type forced 0x20)
    const double q1 = static_cast<double>(raw[1]);
    const double q2 = static_cast<double>(-raw[0]);
    const double q3 = static_cast<double>(raw[2]);
    const double accx = static_cast<double>(raw[4]);
    const double accy = static_cast<double>(-raw[3]);
    const double accz = static_cast<double>(raw[5]);
    const double magx = static_cast<double>(-raw[7]);
    const double magy = static_cast<double>(raw[6]);
    const double magz = static_cast<double>(raw[8]);

    const double q1s = q1 / 16384.0;
    const double q2s = q2 / 16384.0;
    const double q3s = q3 / 16384.0;
    const double n2 = std::min(q1s * q1s + q2s * q2s + q3s * q3s, 1.0);
    const double q0 = std::sqrt(1.0 - n2);

    return {
        q0,
        q1s,
        q2s,
        q3s,
        accx / 8191.0,
        accy / 8191.0,
        accz / 8191.0,
        magx / 6.0,
        magy / 6.0,
        magz / 6.0,
    };
}

EmotivEpocX::EmotivEpocX(bool enable_motion, bool enable_quality, const std::string& record_file)
    : EmotivBase(enable_motion, enable_quality, record_file) {
    device_name = "Emotiv Epoc X";
    KeyModel = 8;
}

EmotivEpocX::~EmotivEpocX() {}

void EmotivEpocX::set_serial_number(const std::string& serial) {
    serial_number = serial;
    ctx_initialized = false;
}

void EmotivEpocX::set_serial_source(const std::string& source) {
    serial_source_ = source;
    ctx_initialized = false;
}

void EmotivEpocX::set_eeg_resolution_bits(int bits) {
    eeg_resolution_bits_ = bits;
}

double EmotivEpocX::adc_div() const {
    return eeg_resolution_bits_ == 0x10 ? kAdcDiv16Bit : kAdcDiv14Bit;
}

std::vector<uint8_t> EmotivEpocX::get_crypto_key() {
    if (serial_number.empty()) {
        hid_device* dev = get_hid_device();
        hid_close(dev);
    }
    const std::string& serial = serial_number;
    if (serial.size() < 4) {
        throw std::runtime_error("Serial number too short to derive key");
    }

    std::array<uint8_t, 4> s4;
    if (serial_source_ == "ble_hex") {
        s4 = serial_bytes_ble_hex(serial);
    } else {
        s4 = serial_bytes_hid(serial);
    }
    const auto key = aes_key_epocx_mode(s4);
    return std::vector<uint8_t>(key.begin(), key.end());
}

std::string EmotivEpocX::get_lsl_source_id() {
    std::vector<uint8_t> key = get_crypto_key();
    std::string hex_key;
    const char* hex_chars = "0123456789abcdef";
    for (uint8_t b : key) {
        hex_key += hex_chars[b >> 4];
        hex_key += hex_chars[b & 0x0F];
    }
    return device_name + "_" + std::to_string(KeyModel) + "_" + hex_key;
}

lsl::stream_info EmotivEpocX::get_lsl_outlet_motion_stream_info() {
    lsl::stream_info info("Epoc X Motion", "SIGNAL", 10, MOTION_SRATE, lsl::cf_float32, get_lsl_source_id());
    lsl::xml_element chns = info.desc().append_child("channels");

    const std::vector<std::string> quat = {"Q0", "Q1", "Q2", "Q3"};
    for (const auto& label : quat) {
        lsl::xml_element ch = chns.append_child("channel");
        ch.append_child_value("label", label);
        ch.append_child_value("unit", "normalized");
        ch.append_child_value("type", "QUAT");
        ch.append_child_value("scaling_factor", "16384");
    }
    const std::vector<std::string> acc = {"AccX", "AccY", "AccZ"};
    for (const auto& label : acc) {
        lsl::xml_element ch = chns.append_child("channel");
        ch.append_child_value("label", label);
        ch.append_child_value("unit", "g");
        ch.append_child_value("type", "ACC");
        ch.append_child_value("scaling_factor", "8191");
    }
    const std::vector<std::string> mag = {"MagX", "MagY", "MagZ"};
    for (const auto& label : mag) {
        lsl::xml_element ch = chns.append_child("channel");
        ch.append_child_value("label", label);
        ch.append_child_value("unit", "uT");
        ch.append_child_value("type", "MAG");
        ch.append_child_value("scaling_factor", "6");
    }
    info.desc().append_child_value(
        "motion_layout",
        "Android Epoc X 20B BE int16: Q/ACC/MAG after correctMotionDirection");
    return add_lsl_outlet_info_common(info);
}

lsl::stream_info EmotivEpocX::get_lsl_outlet_eeg_stream_info() {
    lsl::stream_info info("Epoc X", "EEG", eeg_channel_names.size(), SRATE, lsl::cf_float32, get_lsl_source_id());
    lsl::xml_element chns = info.desc().append_child("channels");
    for (const auto& label : eeg_channel_names) {
        lsl::xml_element ch = chns.append_child("channel");
        ch.append_child_value("label", label);
        ch.append_child_value("unit", "microvolts");
        ch.append_child_value("type", "EEG");
        ch.append_child_value("scaling_factor", "1");
    }
    lsl::xml_element cap = info.desc().append_child("cap");
    cap.append_child_value("name", "easycap-M1");
    cap.append_child_value("labelscheme", "10-20");

    info.desc().append_child_value("adc_div", std::to_string(adc_div()));
    info.desc().append_child_value(
        "adc_scale_note",
        "Mode-7 16-bit default: sample_uV = LE16_word / 7.8 "
        "(14-bit uses /1.95). CyKit convertEPOC_PLUS ≈ LE16/7.8.");
    info.desc().append_child_value(
        "eeg_layout",
        "Mode-7 EpocPlus LE16 + getRawEegId remap; word7 is CQ");
    return add_lsl_outlet_info_common(info);
}

lsl::stream_info EmotivEpocX::get_lsl_outlet_electrode_quality_stream_info() {
    lsl::stream_info info("Epoc X eQuality", "Raw", eeg_channel_names.size(), SRATE, lsl::cf_float32,
                          get_lsl_source_id());
    lsl::xml_element chns = info.desc().append_child("channels");
    for (const auto& label : eeg_quality_channel_names()) {
        lsl::xml_element ch = chns.append_child("channel");
        ch.append_child_value("label", label);
        ch.append_child_value("unit", "cq");
        ch.append_child_value("type", "CQ");
        ch.append_child_value("scaling_factor", "1");
    }
    lsl::xml_element cap = info.desc().append_child("cap");
    cap.append_child_value("name", "easycap-M1");
    cap.append_child_value("labelscheme", "10-20");
    info.desc().append_child_value(
        "cq_note",
        "Mode-7: CQ = word7 & 0x7f (broadcast to all quality channels)");
    return add_lsl_outlet_info_common(info);
}

EmotivData EmotivEpocX::decode_data(const std::vector<uint8_t>& data) {
    EmotivData result;
    const size_t n = data.size();

    if (n == 20) {
        if (!enable_motion_data) {
            return result;
        }
        result.motion_data = decode_motion_20(data);
        if (!result.motion_data.empty()) {
            result.has_motion = true;
            has_motion_data = true;
        }
        return result;
    }

    if (n < 32) {
        return result;
    }

    if (!ctx_initialized) {
        std::vector<uint8_t> key = get_crypto_key();
        AES_init_ctx(&ctx, key.data());
        ctx_initialized = true;
    }

    std::vector<uint8_t> plain = data;
    plain.resize(32);
    for (size_t i = 0; i < 32; ++i) {
        plain[i] ^= 0x55;
    }
    AES_ECB_decrypt(&ctx, plain.data());
    AES_ECB_decrypt(&ctx, plain.data() + 16);

    return decode_eeg_mode7(plain, enable_electrode_quality_stream, adc_div());
}

bool EmotivEpocX::validate_data(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    if (is_reverse_engineer_mode) {
        return n == static_cast<size_t>(READ_SIZE);
    }
    return n == 20 || n == 32;
}
