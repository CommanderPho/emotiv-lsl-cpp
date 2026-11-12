#pragma once
#include "emotiv_base.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Android aesKeyVersion2_EpocXMode NEON tbl mask (libEmbeddedLibQt VA 0xc413a0).
inline constexpr std::array<int, 16> kEpocXModePerm = {
    0, 1, 3, 3, 1, 0, 1, 3, 0, 3, 2, 1, 0, 1, 1, 2};

// Android getRawEegId for HeadsetType 7, mode 7 (skips word 7 / CQ).
inline constexpr std::array<int, 14> kEpocXRawEegIdMode7 = {
    2, 3, 0, 1, 4, 5, 6, 8, 9, 10, 13, 14, 11, 12};

// Android convertADCToMicrovolt: 16-bit → ÷7.8, 14-bit → ÷1.95.
inline constexpr double kAdcDiv16Bit = 7.8;
inline constexpr double kAdcDiv14Bit = 1.95;

inline constexpr uint8_t kEegType = 0x10;
inline constexpr uint8_t kMotionType = 0x20;

/** USB HID / CyKit material: s[0..3] = ASCII of serial[-1], [-2], [-3], [-4]. */
std::array<uint8_t, 4> serial_bytes_hid(const std::string& serial);

/** BLE-style decrypt serial: hex-decoded DIS/name bytes, first four only. */
std::array<uint8_t, 4> serial_bytes_ble_hex(const std::string& serial_hex);

/** Android Version2 EpocXMode AES-128 key from first four decrypt-serial bytes. */
std::array<uint8_t, 16> aes_key_epocx_mode(const std::array<uint8_t, 4>& s4);

/** XOR 0x55 then AES-128-ECB decrypt of a 32-byte EEG cipher report. */
std::vector<uint8_t> decrypt_eeg_packet(const std::vector<uint8_t>& data, const uint8_t* key16);

/**
 * Mode-7 EEG: LE16 words → getRawEegId remap → ADC scale; optional CQ from word7.
 * Returns empty EEG when type is not 0x10.
 */
EmotivData decode_eeg_mode7(const std::vector<uint8_t>& plain, bool enable_quality,
                            double adc_div = kAdcDiv16Bit);

/**
 * Android convert_pkgs_motion + correctMotionDirection + physical scale.
 * Expects a 20-byte motion package (pass-through for fw <= 0x73e).
 * Returns [Q0, Q1, Q2, Q3, AccX, AccY, AccZ, MagX, MagY, MagZ].
 */
std::vector<double> decode_motion_20(const std::vector<uint8_t>& data);

class EmotivEpocX : public EmotivBase {
public:
    EmotivEpocX(bool enable_motion_data = false, bool enable_electrode_quality_stream = false,
                const std::string& record_file = "");
    ~EmotivEpocX();

    /** Test / offline: inject serial without opening HID. */
    void set_serial_number(const std::string& serial);

    /** 'hid' (default) or 'ble_hex' serial material for get_crypto_key. */
    void set_serial_source(const std::string& source);

    /** Android EEG resolution bits (+0xb4): 0x10 → ÷7.8, else ÷1.95. */
    void set_eeg_resolution_bits(int bits);

    std::vector<uint8_t> get_crypto_key() override;

    EmotivData decode_data(const std::vector<uint8_t>& data) override;
    bool validate_data(const std::vector<uint8_t>& data) override;

protected:
    void apply_replay_header(const EpocXWireHeader& header) override;
    std::string get_lsl_source_id() override;

    lsl::stream_info get_lsl_outlet_eeg_stream_info() override;
    lsl::stream_info get_lsl_outlet_motion_stream_info() override;
    lsl::stream_info get_lsl_outlet_electrode_quality_stream_info() override;

private:
    std::string serial_source_ = "hid";
    int eeg_resolution_bits_ = 0x10;

    double adc_div() const;
};
