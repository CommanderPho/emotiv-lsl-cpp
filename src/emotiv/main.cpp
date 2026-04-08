#include <iostream>
#include "emotiv_epoc_x.h"
#include "emotiv_lsl_log_config.h"
#include "lsltemplate/Config.hpp"

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

        if (record_file.empty()) {
            const auto cfg_path = lsltemplate::ConfigManager::findConfigFile("LSLTemplate.cfg");
            if (!cfg_path.empty()) {
                const auto cfg = lsltemplate::ConfigManager::load(cfg_path);
                if (cfg) {
                    auto exe_dir = lsltemplate::ConfigManager::executableDirectory();
                    const auto resolved = lsltemplate::ConfigManager::resolveRecordingOutputPath(*cfg, exe_dir);
                    if (resolved) {
                        record_file = resolved->string();
                    }
                }
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
