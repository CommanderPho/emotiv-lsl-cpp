#include <iostream>
#include "emotiv_epoc_x.h"
#include "emotiv_lsl_log_config.h"

int main(int argc, char* argv[]) {
    emotiv_set_lslapicfg_from_exe_dir();
    try {
        bool enable_quality = true;
        bool enable_motion = true;
        std::string record_file = "";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--record" && i + 1 < argc) {
                record_file = argv[++i];
            }
        }

        std::cout << "Starting Emotiv LSL C++ Server..." << std::endl;
        if (!record_file.empty()) {
            std::cout << "Recording natively to XDF file: " << record_file << std::endl;
        }
        
        EmotivEpocX epocX(enable_motion, enable_quality, record_file);
        
        epocX.main_loop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
