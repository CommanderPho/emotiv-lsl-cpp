#pragma once
#include "emotiv_base.h"

class EmotivEpocPlus : public EmotivBase {
public:
    EmotivEpocPlus(bool enable_electrode_quality_stream = false, bool is_fourteen_bit_mode = false, const std::string& record_file = "");
    ~EmotivEpocPlus();

protected:
    std::vector<uint8_t> get_crypto_key() override;

    EmotivData decode_data(const std::vector<uint8_t>& data) override;
    bool validate_data(const std::vector<uint8_t>& data) override;

private:
    bool is_fourteen_bit_mode;
};
