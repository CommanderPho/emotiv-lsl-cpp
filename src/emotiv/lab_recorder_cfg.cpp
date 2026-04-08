#include "lab_recorder_cfg.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif
#endif

namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::map<std::string, std::string> read_ini_flat(const std::filesystem::path& path) {
    std::map<std::string, std::string> kv;
    std::ifstream file(path);
    if (!file.is_open()) {
        return kv;
    }
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        kv[key] = val;
    }
    return kv;
}

std::filesystem::path get_documents_directory() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, buf))) {
        return std::filesystem::path(buf);
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Documents";
    }
#else
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Documents";
    }
#endif
    return std::filesystem::current_path();
}

std::string hostname_string() {
#if defined(_WIN32)
    char buf[256];
    DWORD n = sizeof(buf);
    if (GetComputerNameA(buf, &n)) {
        return std::string(buf, n);
    }
#else
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof(buf)) == 0) {
        return std::string(buf);
    }
#endif
    return "unknown-host";
}

std::string iso8601_utc_ms() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t t = system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    if (gmtime_s(&utc, &t) != 0) {
        return {};
    }
#else
    if (gmtime_r(&t, &utc) == nullptr) {
        return {};
    }
#endif
    std::ostringstream o;
    o << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return o.str();
}

std::string local_date_yyyy_mm_dd() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32)
    if (localtime_s(&local_tm, &t) != 0) {
        return {};
    }
#else
    if (localtime_r(&t, &local_tm) == nullptr) {
        return {};
    }
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &local_tm) == 0) {
        return {};
    }
    return std::string(buf);
}

std::string local_time_hh_mm_ss() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32)
    if (localtime_s(&local_tm, &t) != 0) {
        return {};
    }
#else
    if (localtime_r(&t, &local_tm) == nullptr) {
        return {};
    }
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%H-%M-%S", &local_tm) == 0) {
        return {};
    }
    return std::string(buf);
}

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos;) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

struct Defaults {
    std::string block = "Default";
    std::string participant = "P001";
    std::string session = "S001";
    std::string acq = "";
    std::string modality = "eeg";
};

std::string expand_placeholders(std::string tpl, const Defaults& d, int counter, bool use_run_r) {
    const std::string dt = iso8601_utc_ms();
    replace_all(tpl, "%datetime_eeg", dt);
    replace_all(tpl, "%datetime", dt);
    replace_all(tpl, "%date", local_date_yyyy_mm_dd());
    replace_all(tpl, "%time", local_time_hh_mm_ss());
    replace_all(tpl, "%hostname", hostname_string());

    replace_all(tpl, "%b", d.block);
    replace_all(tpl, "%p", d.participant);
    replace_all(tpl, "%s", d.session);
    replace_all(tpl, "%a", d.acq);
    replace_all(tpl, "%m", d.modality);

    std::ostringstream run_os;
    run_os << std::setw(3) << std::setfill('0') << counter;
    const std::string run = run_os.str();
    if (use_run_r) {
        replace_all(tpl, "%r", run);
    }
    if (!use_run_r) {
        replace_all(tpl, "%n", run);
    }
    return tpl;
}

bool template_has_counter(const std::string& tpl, bool bids) {
    return bids ? (tpl.find("%r") != std::string::npos) : (tpl.find("%n") != std::string::npos);
}

std::filesystem::path normalize_join(std::filesystem::path root, std::string rel) {
    for (char& c : rel) {
        if (c == '\\') {
            c = '/';
        }
    }
    while (!rel.empty() && rel.front() == '/') {
        rel.erase(rel.begin());
    }
    return (root / rel).lexically_normal();
}

} // namespace

std::optional<std::filesystem::path> find_lab_recorder_config_file(
    const std::optional<std::filesystem::path>& explicit_path,
    const std::filesystem::path& emotiv_exe_dir) {
    if (explicit_path && !explicit_path->empty()) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(*explicit_path, ec)) {
            return std::filesystem::weakly_canonical(*explicit_path, ec);
        }
        return std::nullopt;
    }

    constexpr const char* kName = "LabRecorder.cfg";
    std::vector<std::filesystem::path> dirs;
    dirs.push_back(std::filesystem::current_path());
#if defined(_WIN32)
    if (const char* p = std::getenv("LOCALAPPDATA")) {
        dirs.push_back(std::filesystem::path(p) / "LabRecorder");
    }
    if (const char* p = std::getenv("APPDATA")) {
        dirs.push_back(std::filesystem::path(p) / "LabRecorder");
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        dirs.push_back(std::filesystem::path(home) / "Library" / "Preferences" / "LabRecorder");
        dirs.push_back(std::filesystem::path(home) / "Library" / "Application Support" / "LabRecorder");
    }
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        dirs.push_back(std::filesystem::path(xdg) / "LabRecorder");
    } else if (const char* home = std::getenv("HOME")) {
        dirs.push_back(std::filesystem::path(home) / ".config" / "LabRecorder");
    }
    if (const char* home = std::getenv("HOME")) {
        dirs.push_back(std::filesystem::path(home) / ".local" / "share" / "LabRecorder");
    }
#endif
    if (!emotiv_exe_dir.empty()) {
        dirs.push_back(emotiv_exe_dir);
    }

    std::error_code ec;
    for (const auto& dir : dirs) {
        if (dir.empty()) {
            continue;
        }
        const auto candidate = dir / kName;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return std::filesystem::weakly_canonical(candidate, ec);
        }
    }
    return std::nullopt;
}

static bool strip_study_root_prefix(const std::string& study_root_norm, std::string* str_path) {
    std::filesystem::path srp(study_root_norm);
    const std::string sr = srp.generic_string();
    std::string& sp = *str_path;
    std::filesystem::path raw(sp);
    const std::string spg = raw.generic_string();
    if (spg.size() > sr.size() && spg.compare(0, sr.size(), sr) == 0 &&
        (spg[sr.size()] == '/' || spg[sr.size()] == '\\')) {
        sp = spg.substr(sr.size() + 1);
        return true;
    }
    return false;
}

std::optional<std::filesystem::path> resolve_lab_recorder_output_path(
    const std::filesystem::path& labrec_cfg_path) {
    const auto kv = read_ini_flat(labrec_cfg_path);
    std::string study_root;
    std::string legacy_template;
    const bool has_storage =
        kv.find("StorageLocation") != kv.end() && !kv.at("StorageLocation").empty();
    const bool has_study = kv.find("StudyRoot") != kv.end() && !kv.at("StudyRoot").empty();
    const bool has_path_tpl = kv.find("PathTemplate") != kv.end() && !kv.at("PathTemplate").empty();

    if (has_storage) {
        if (has_study || has_path_tpl) {
            return std::nullopt;
        }
        const std::string str_path = kv.at("StorageLocation");
        const size_t index = str_path.find('%');
        const std::string path_root = index != std::string::npos ? str_path.substr(0, index) : str_path;
        std::filesystem::path pr(path_root);
        std::error_code ec;
        const std::filesystem::path study_root_path = std::filesystem::absolute(pr, ec).parent_path();
        if (ec) {
            return std::nullopt;
        }
        study_root = study_root_path.string();
        std::string remainder = str_path;
        if (!strip_study_root_prefix(study_root_path.generic_string(), &remainder)) {
            const std::string sr_alt = study_root_path.string();
            remainder = str_path;
            if (!strip_study_root_prefix(sr_alt, &remainder)) {
                return std::nullopt;
            }
        }
        legacy_template = remainder;
    } else {
        if (has_study) {
            study_root = kv.at("StudyRoot");
        }
        if (has_path_tpl) {
            legacy_template = kv.at("PathTemplate");
        }
    }

    if (study_root.empty()) {
        study_root = (get_documents_directory() / "CurrentStudy").string();
    } else {
        std::error_code ec;
        study_root = std::filesystem::absolute(std::filesystem::path(study_root), ec).string();
        if (ec) {
            return std::nullopt;
        }
    }

    const bool bids_default = legacy_template.empty();
    if (bids_default) {
        legacy_template = "sub-%p/ses-%s/%m/sub-%p_ses-%s_task-%b_run-%r_%m.xdf";
    }

    const bool bids = bids_default || (legacy_template.find("sub-%p") != std::string::npos);
    Defaults d;
    const bool use_run_r = bids;

    if (!template_has_counter(legacy_template, use_run_r)) {
        std::string expanded = expand_placeholders(legacy_template, d, 1, use_run_r);
        std::filesystem::path full = normalize_join(std::filesystem::path(study_root), expanded);
        std::error_code ec;
        std::filesystem::create_directories(full.parent_path(), ec);
        if (ec) {
            return std::nullopt;
        }
        return std::filesystem::absolute(full);
    }

    for (int i = 1; i <= 1000; ++i) {
        std::string expanded = expand_placeholders(legacy_template, d, i, use_run_r);
        std::filesystem::path full = normalize_join(std::filesystem::path(study_root), expanded);
        std::error_code ec;
        if (!std::filesystem::exists(full, ec)) {
            std::filesystem::create_directories(full.parent_path(), ec);
            if (ec) {
                return std::nullopt;
            }
            return std::filesystem::absolute(full);
        }
    }
    return std::nullopt;
}
