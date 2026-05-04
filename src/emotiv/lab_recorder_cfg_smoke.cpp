/// Deterministic checks for LabRecorder.cfg path parity (no hardware / headset).
#include "lab_recorder_cfg.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <stdlib.h>
#endif

static int fail(const char* msg) {
    std::cerr << "lab_recorder_cfg_smoke: " << msg << std::endl;
    return 1;
}

static void set_fixed_epoch_ms(long long ms) {
    const std::string v = std::to_string(ms);
#if defined(_WIN32)
    _putenv_s("EMOTIV_LABREC_FIXED_UNIX_MS", v.c_str());
#else
    setenv("EMOTIV_LABREC_FIXED_UNIX_MS", v.c_str(), 1);
#endif
}

int main() {
    set_fixed_epoch_ms(946684800000LL);

    {
        const std::string r = lab_recorder_replace_filename(
            "LabRecorder_%hostname_%datetime_eeg.xdf", "Default", "P001", "S001", "", "eeg", 1, true);
        if (r.size() < 8) {
            return fail("datetime_eeg template produced unexpected short string");
        }
        if (r.find("_eeg.xdf") == std::string::npos) {
            return fail("expected literal _eeg.xdf after Qt-style %datetime prefix replace");
        }
    }

    {
        const std::string r = lab_recorder_replace_filename(
            "exp%n/block_%b.xdf", "BlockOne", "P001", "S001", "", "eeg", 7, false);
        if (r != "exp007/block_BlockOne.xdf") {
            return fail("legacy %n / %b expansion mismatch");
        }
    }

    {
        const std::string r = lab_recorder_replace_filename(
            "sub-%p/ses-%s_task-%b_run-%r_%m.xdf", "B", "PX", "SX", "", "eeg", 3, true);
        if (r != "sub-PX/ses-SX_task-B_run-003_eeg.xdf") {
            return fail("BIDS %r expansion mismatch");
        }
    }

    {
        const auto tmp = std::filesystem::temp_directory_path() / "emotiv_lr_cfg_smoke";
        std::filesystem::create_directories(tmp);
        const auto cfg = tmp / "LabRecorder.cfg";
        {
            std::ofstream o(cfg);
            o << "StudyRoot=" << tmp.string() << "\n";
            o << "PathTemplate=subdir/rec_%n.xdf\n";
        }
        const auto resolved = resolve_lab_recorder_output_path(cfg);
        if (!resolved) {
            return fail("resolve_lab_recorder_output_path returned nullopt");
        }
        if (resolved->string().find("rec_001.xdf") == std::string::npos) {
            return fail("expected first free legacy counter 001 in filename");
        }
    }

    std::cout << "lab_recorder_cfg_smoke: ok" << std::endl;
    return 0;
}
