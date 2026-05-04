#include <filesystem>
#include <iostream>
#include <optional>
#include "emotiv_epoc_x.h"
#include "emotiv_lsl_log_config.h"
#include "lab_recorder_cfg.h"
#include "shutdown_hooks.h"
#include "lsltemplate/Config.hpp"

class EmotivBase;

namespace {
struct EmotivShutdownScope {
    explicit EmotivShutdownScope(EmotivBase* base) { install_emotiv_shutdown_handlers(base); }
    ~EmotivShutdownScope() { remove_emotiv_shutdown_handlers(); }
};
} // namespace

int main(int argc, char* argv[]) {
    emotiv_set_lslapicfg_from_exe_dir();
    try {
        bool enable_quality = true;
        bool enable_motion = true;
        std::string record_file = "";
        std::optional<std::filesystem::path> explicit_labrec_cfg;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--record" && i + 1 < argc) {
                record_file = argv[++i];
            } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                explicit_labrec_cfg = std::filesystem::path(argv[++i]);
            }
        }

        if (record_file.empty()) {
            const auto exe_dir = lsltemplate::ConfigManager::executableDirectory();
            const auto lr_cfg = find_lab_recorder_config_file(explicit_labrec_cfg, exe_dir);
            if (lr_cfg) {
                const auto resolved = resolve_lab_recorder_output_path(*lr_cfg);
                if (resolved) {
                    record_file = resolved->string();
                }
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

        EmotivShutdownScope shutdown_scope(&epocX);
        epocX.main_loop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
