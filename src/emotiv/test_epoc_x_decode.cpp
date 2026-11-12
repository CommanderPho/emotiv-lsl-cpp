/** Offline assertions for Epoc X Mode-7 key + decode (no headset required). */

#include "emotiv_epoc_x.h"
#include "epocx_wire_replay.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifndef EPOCX_TESTDATA_DIR
#define EPOCX_TESTDATA_DIR "."
#endif

extern "C" {
#include "aes.h"
}

namespace {

int g_failures = 0;

void expect_true(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << std::endl;
        ++g_failures;
    }
}

void expect_eq_u8(uint8_t a, uint8_t b, const char* msg) {
    if (a != b) {
        std::cerr << "FAIL: " << msg << " (" << int(a) << " != " << int(b) << ")" << std::endl;
        ++g_failures;
    }
}

void expect_near(double a, double b, double tol, const char* msg) {
    if (std::fabs(a - b) > tol) {
        std::cerr << "FAIL: " << msg << " (" << a << " vs " << b << ")" << std::endl;
        ++g_failures;
    }
}

std::vector<uint8_t> build_mode7_plain(uint8_t counter, uint8_t type_byte, const uint16_t words[15]) {
    std::vector<uint8_t> plain(32, 0);
    plain[0] = counter;
    plain[1] = type_byte;
    for (int i = 0; i < 15; ++i) {
        plain[2 + 2 * i] = static_cast<uint8_t>(words[i] & 0xff);
        plain[2 + 2 * i + 1] = static_cast<uint8_t>((words[i] >> 8) & 0xff);
    }
    return plain;
}

std::vector<uint8_t> encrypt_wire(const std::vector<uint8_t>& plain32, const uint8_t* key16) {
    std::vector<uint8_t> buf = plain32;
    buf.resize(32);
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, key16);
    AES_ECB_encrypt(&ctx, buf.data());
    AES_ECB_encrypt(&ctx, buf.data() + 16);
    for (size_t i = 0; i < 32; ++i) {
        buf[i] ^= 0x55;
    }
    return buf;
}

void write_be_i16(uint8_t* p, int16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xff);
    p[1] = static_cast<uint8_t>(v & 0xff);
}

void test_perm_and_keys() {
    expect_true(kEpocXModePerm.size() == 16, "perm length");

    auto s4 = serial_bytes_ble_hex("E50202E9");
    expect_eq_u8(s4[0], 0xE5, "ble s0");
    expect_eq_u8(s4[1], 0x02, "ble s1");
    expect_eq_u8(s4[2], 0x02, "ble s2");
    expect_eq_u8(s4[3], 0xE9, "ble s3");

    auto key = aes_key_epocx_mode(s4);
    const uint8_t expected[] = {
        0xE5, 0x02, 0xE9, 0xE9, 0x02, 0xE5, 0x02, 0xE9,
        0xE5, 0xE9, 0x02, 0x02, 0xE5, 0x02, 0x02, 0x02,
    };
    for (size_t i = 0; i < 16; ++i) {
        expect_eq_u8(key[i], expected[i], "ble EpocXMode key byte");
    }

    const std::string serial = "UD20221202006756";
    auto hid4 = serial_bytes_hid(serial);
    expect_eq_u8(hid4[0], '6', "hid s0");
    expect_eq_u8(hid4[1], '5', "hid s1");
    expect_eq_u8(hid4[2], '7', "hid s2");
    expect_eq_u8(hid4[3], '6', "hid s3");

    auto hid_key = aes_key_epocx_mode(hid4);
    // Same schedule as historical get_crypto_key index list.
    const char* sn = serial.c_str();
    const size_t len = serial.size();
    const uint8_t legacy[] = {
        static_cast<uint8_t>(sn[len - 1]),
        static_cast<uint8_t>(sn[len - 2]),
        static_cast<uint8_t>(sn[len - 4]),
        static_cast<uint8_t>(sn[len - 4]),
        static_cast<uint8_t>(sn[len - 2]),
        static_cast<uint8_t>(sn[len - 1]),
        static_cast<uint8_t>(sn[len - 2]),
        static_cast<uint8_t>(sn[len - 4]),
        static_cast<uint8_t>(sn[len - 1]),
        static_cast<uint8_t>(sn[len - 4]),
        static_cast<uint8_t>(sn[len - 3]),
        static_cast<uint8_t>(sn[len - 2]),
        static_cast<uint8_t>(sn[len - 1]),
        static_cast<uint8_t>(sn[len - 2]),
        static_cast<uint8_t>(sn[len - 2]),
        static_cast<uint8_t>(sn[len - 3]),
    };
    for (size_t i = 0; i < 16; ++i) {
        expect_eq_u8(hid_key[i], legacy[i], "hid EpocXMode == legacy index list");
    }

    EmotivEpocX hs(false, false, "");
    hs.set_serial_number(serial);
    hs.set_serial_source("hid");
    auto device_key = hs.get_crypto_key();
    expect_true(device_key.size() == 16, "device key size");
    for (size_t i = 0; i < 16; ++i) {
        expect_eq_u8(device_key[i], hid_key[i], "device get_crypto_key hid");
    }
}

void test_mode7_decode() {
    auto key_arr = aes_key_epocx_mode(serial_bytes_ble_hex("E50202E9"));

    uint16_t words[15];
    for (int i = 0; i < 15; ++i) {
        words[i] = static_cast<uint16_t>(1000 + i);
    }
    auto plain = build_mode7_plain(3, 0x10, words);
    auto decoded = decode_eeg_mode7(plain, true, kAdcDiv16Bit);
    expect_true(decoded.has_eeg, "has eeg");
    expect_true(decoded.eeg_data.size() == 14, "14 channels");
    for (size_t i = 0; i < 14; ++i) {
        const double expected = static_cast<double>(words[kEpocXRawEegIdMode7[i]]) / kAdcDiv16Bit;
        expect_near(decoded.eeg_data[i], expected, 1e-9, "remap+adc");
    }
    expect_true(decoded.has_quality, "has cq");
    expect_near(decoded.quality_data[0], static_cast<double>(words[7] & 0x7f), 1e-9, "cq word7");

    uint16_t zeros[15] = {};
    auto skip = decode_eeg_mode7(build_mode7_plain(0, 0x20, zeros), true, kAdcDiv16Bit);
    expect_true(!skip.has_eeg && !skip.has_quality, "skip non-eeg type");

    words[7] = 0x12AB;
    for (int i = 0; i < 15; ++i) {
        if (i != 7) {
            words[i] = static_cast<uint16_t>(100 + i * 17);
        }
    }
    plain = build_mode7_plain(5, 0x10, words);
    auto wire = encrypt_wire(plain, key_arr.data());

    EmotivEpocX hs(true, true, "");
    hs.set_serial_number("E50202E9");
    hs.set_serial_source("ble_hex");
    auto roundtrip = hs.decode_data(wire);
    expect_true(roundtrip.has_eeg, "encrypted roundtrip eeg");
    for (size_t i = 0; i < 14; ++i) {
        const double expected = static_cast<double>(words[kEpocXRawEegIdMode7[i]]) / kAdcDiv16Bit;
        expect_near(roundtrip.eeg_data[i], expected, 1e-9, "encrypted remap");
    }
    expect_near(roundtrip.quality_data[0], static_cast<double>(0x12AB & 0x7f), 1e-9, "encrypted cq");
}

void test_motion_20() {
    int16_t raws[9] = {1000, -2000, 3000, 4000, -5000, 6000, 7, -8, 9};
    std::vector<uint8_t> buf(20, 0);
    buf[0] = 1;
    buf[1] = 0x20;
    for (int i = 0; i < 9; ++i) {
        write_be_i16(buf.data() + 2 + 2 * i, raws[i]);
    }

    auto out = decode_motion_20(buf);
    expect_true(out.size() == 10, "motion 10 ch");

    const double q1 = raws[1] / 16384.0;
    const double q2 = (-raws[0]) / 16384.0;
    const double q3 = raws[2] / 16384.0;
    const double n2 = std::min(q1 * q1 + q2 * q2 + q3 * q3, 1.0);
    const double q0 = std::sqrt(1.0 - n2);
    const double expected[10] = {
        q0,
        q1,
        q2,
        q3,
        raws[4] / 8191.0,
        (-raws[3]) / 8191.0,
        raws[5] / 8191.0,
        (-raws[7]) / 6.0,
        raws[6] / 6.0,
        raws[8] / 6.0,
    };
    for (size_t i = 0; i < 10; ++i) {
        expect_near(out[i], expected[i], 1e-9, "motion scale/axes");
    }

    EmotivEpocX hs(true, false, "");
    hs.set_serial_number("00000000");
    hs.set_serial_source("ble_hex");
    auto routed = hs.decode_data(buf);
    expect_true(routed.has_motion && routed.motion_data.size() == 10, "decode_data routes 20B");
    expect_true(!routed.has_eeg, "20B is not eeg");
}

void test_no_cykit_gyro_heuristic() {
    // HID last-4 of "abcd" → d,c,b,a
    auto key_arr = aes_key_epocx_mode(serial_bytes_hid("abcd"));
    uint16_t zeros[15] = {};
    auto plain = build_mode7_plain(0, 0x20, zeros);
    auto wire = encrypt_wire(plain, key_arr.data());

    EmotivEpocX hs(true, true, "");
    hs.set_serial_number("abcd");
    hs.set_serial_source("hid");
    auto device_key = hs.get_crypto_key();
    for (size_t i = 0; i < 16; ++i) {
        expect_eq_u8(device_key[i], key_arr[i], "heuristic key match");
    }
    auto decoded = hs.decode_data(wire);
    expect_true(!decoded.has_eeg && !decoded.has_motion && !decoded.has_quality,
                "type 0x20 in 32B is not motion");
}

void test_wire_fixture_replay() {
    const std::string path = std::string(EPOCX_TESTDATA_DIR) + "/fixture_mode7.epocxwire";
    EpocXWireReplay replay(path);
    expect_true(replay.header().serial == "UD20221202006756", "fixture serial");
    expect_true(replay.size() == 2, "fixture packet count");

    EmotivEpocX hs(false, true, "");
    hs.enable_wire_replay(std::make_unique<EpocXWireReplay>(path), 0.0);

    // Serial already injected via enable_wire_replay; read packets separately.
    EpocXWireReplay reader(path);
    auto payload = reader.read(0.0);
    expect_true(payload.size() == 32, "first payload 32B");
    auto decoded = hs.decode_data(payload);
    expect_true(decoded.has_eeg, "fixture has eeg");
    expect_true(decoded.eeg_data.size() == 14, "14 ch");
    expect_true(decoded.has_quality, "fixture has cq");
    expect_near(decoded.quality_data[0], 0x2B, 1e-9, "fixture cq");
    // First channel AF3 = 100 µV after quantize.
    expect_near(decoded.eeg_data[0], round(100.0 * 7.8) / 7.8, 1e-9, "fixture AF3");
}

} // namespace

int main() {
    test_perm_and_keys();
    test_mode7_decode();
    test_motion_20();
    test_no_cykit_gyro_heuristic();
    test_wire_fixture_replay();

    if (g_failures != 0) {
        std::cerr << g_failures << " assertion(s) failed" << std::endl;
        return 1;
    }
    std::cout << "All Epoc X decode tests passed" << std::endl;
    return 0;
}
