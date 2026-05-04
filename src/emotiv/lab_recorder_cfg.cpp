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

std::chrono::system_clock::time_point now_for_placeholders() {
    if (const char* e = std::getenv("EMOTIV_LABREC_FIXED_UNIX_MS")) {
        const long long ms = std::atoll(e);
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
    }
    return std::chrono::system_clock::now();
}

/// Qt `QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHHmmss.zzzZ")` for UTC (compact clock, ms, zone).
std::string qt_style_datetime_utc_ms(const std::chrono::system_clock::time_point& tp) {
    using namespace std::chrono;
    const auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    const std::time_t t = system_clock::to_time_t(tp);
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
    o << std::put_time(&utc, "%Y-%m-%dT%H%M%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return o.str();
}

std::string qt_style_date_utc(const std::chrono::system_clock::time_point& tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
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
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &utc) == 0) {
        return {};
    }
    return std::string(buf);
}

/// Qt `nowUtc.toString("HHmmss.zzzZ")` — UTC time-of-day only.
std::string qt_style_time_utc(const std::chrono::system_clock::time_point& tp) {
    using namespace std::chrono;
    const auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    const std::time_t t = system_clock::to_time_t(tp);
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
    o << std::put_time(&utc, "%H%M%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return o.str();
}

std::string hostname_raw() {
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
    return "UNKNOWN-HOST";
}

void sanitize_hostname_like_qt(std::string& h) {
    if (h.empty()) {
        h = "UNKNOWN-HOST";
        return;
    }
    for (char& c : h) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            c = '_';
        }
    }
    for (char& c : h) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            c = '_';
        }
    }
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

/// Mirrors `MainWindow::replaceFilename` order (LabRecorder App-LabRecorder).
void replace_filename_in_place(
    std::string& fullfile,
    const Defaults& d,
    int spin_counter,
    bool bids_checked,
    const std::chrono::system_clock::time_point& now_tp) {
    replace_all(fullfile, "%b", d.block);
    replace_all(fullfile, "%p", d.participant);
    replace_all(fullfile, "%s", d.session);
    replace_all(fullfile, "%a", d.acq);
    replace_all(fullfile, "%m", d.modality);

    std::ostringstream run_os;
    run_os << std::setw(3) << std::setfill('0') << spin_counter;
    const std::string run = run_os.str();
    const char* counter_ph = bids_checked ? "%r" : "%n";
    replace_all(fullfile, counter_ph, run);

    replace_all(fullfile, "%datetime", qt_style_datetime_utc_ms(now_tp));
    replace_all(fullfile, "%date", qt_style_date_utc(now_tp));
    replace_all(fullfile, "%time", qt_style_time_utc(now_tp));

    std::string host = hostname_raw();
    sanitize_hostname_like_qt(host);
    replace_all(fullfile, "%hostname", host);

    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    fullfile.erase(fullfile.begin(), std::find_if(fullfile.begin(), fullfile.end(), not_space));
    fullfile.erase(std::find_if(fullfile.rbegin(), fullfile.rend(), not_space).base(), fullfile.end());
}

std::vector<std::string> parse_session_blocks_value(const std::string& raw) {
    std::vector<std::string> out;
    if (raw.empty() || raw[0] == '@') {
        return out;
    }
    size_t i = 0;
    while (i < raw.size()) {
        while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == ',')) {
            ++i;
        }
        if (i >= raw.size()) {
            break;
        }
        if (raw[i] == '"') {
            ++i;
            std::string item;
            while (i < raw.size() && raw[i] != '"') {
                item += raw[i++];
            }
            if (i < raw.size()) {
                ++i;
            }
            if (!item.empty()) {
                out.push_back(std::move(item));
            }
        } else {
            const size_t start = i;
            while (i < raw.size() && raw[i] != ',') {
                ++i;
            }
            std::string item = trim(raw.substr(start, i - start));
            if (!item.empty()) {
                out.push_back(std::move(item));
            }
        }
    }
    return out;
}

bool template_has_counter(const std::string& tpl, bool use_run_r) {
    return use_run_r ? (tpl.find("%r") != std::string::npos) : (tpl.find("%n") != std::string::npos);
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

void rename_existing_file_labrecorder_style(const std::filesystem::path& target) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(target, ec)) {
        return;
    }
    const auto parent = target.parent_path();
    const auto stem = target.stem().string();
    const auto ext = target.extension().string();
    for (int i = 1; i < 10000; ++i) {
        const auto candidate = parent / (stem + "_old" + std::to_string(i) + ext);
        if (!std::filesystem::exists(candidate, ec)) {
            std::filesystem::rename(target, candidate, ec);
            return;
        }
    }
}

} // namespace

std::string lab_recorder_replace_filename(
    std::string fullfile,
    const std::string& block,
    const std::string& participant,
    const std::string& session,
    const std::string& acq,
    const std::string& modality,
    int spin_counter,
    bool bids_checked) {
    Defaults d;
    d.block = block;
    d.participant = participant;
    d.session = session;
    d.acq = acq;
    d.modality = modality;
    const auto now_tp = now_for_placeholders();
    replace_filename_in_place(fullfile, d, spin_counter, bids_checked, now_tp);
    return fullfile;
}

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
    {
        char buf[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf))) {
            dirs.push_back(std::filesystem::path(buf) / "LabRecorder");
        }
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

std::optional<std::filesystem::path> resolve_lab_recorder_output_path(
    const std::filesystem::path& labrec_cfg_path) {
    const auto kv = read_ini_flat(labrec_cfg_path);
    std::string study_root;
    std::string legacy_template;
    const bool has_storage =
        kv.find("StorageLocation") != kv.end() && !kv.at("StorageLocation").empty();
    const bool has_study = kv.find("StudyRoot") != kv.end() && !kv.at("StudyRoot").empty();
    const bool has_path_tpl = kv.find("PathTemplate") != kv.end() && !kv.at("PathTemplate").empty();
    const bool had_explicit_path_template = has_path_tpl || has_storage;

    if (has_storage) {
        if (has_study || has_path_tpl) {
            return std::nullopt;
        }
        const std::string str_path = kv.at("StorageLocation");
        const size_t index = str_path.find('%');
        const std::string path_root = index != std::string::npos ? str_path.substr(0, index) : str_path;
        std::error_code ec;
        const std::filesystem::path abs_path_root = std::filesystem::absolute(std::filesystem::path(path_root), ec);
        if (ec) {
            return std::nullopt;
        }
        std::filesystem::path study_root_path;
        if (std::filesystem::exists(abs_path_root, ec) && std::filesystem::is_directory(abs_path_root, ec)) {
            study_root_path = abs_path_root;
        } else {
            study_root_path = abs_path_root.parent_path();
        }
        study_root = study_root_path.string();

        const std::string sr_gen = study_root_path.generic_string();
        std::string str_norm = str_path;
        for (char& c : str_norm) {
            if (c == '\\') {
                c = '/';
            }
        }
        if (str_norm.size() > sr_gen.size() + 1 && str_norm.compare(0, sr_gen.size(), sr_gen) == 0 && str_norm[sr_gen.size()] == '/') {
            legacy_template = str_norm.substr(sr_gen.size() + 1);
        } else {
            std::string sr2 = study_root_path.string();
            for (char& c : sr2) {
                if (c == '\\') {
                    c = '/';
                }
            }
            if (str_norm.size() > sr2.size() + 1 && str_norm.compare(0, sr2.size(), sr2) == 0 && str_norm[sr2.size()] == '/') {
                legacy_template = str_norm.substr(sr2.size() + 1);
            } else {
                return std::nullopt;
            }
        }
    } else {
        if (has_study) {
            study_root = kv.at("StudyRoot");
        }
        if (has_path_tpl) {
            legacy_template = kv.at("PathTemplate");
        }
    }

    const bool legacy_was_empty_initial = legacy_template.empty();
    if (study_root.empty()) {
        study_root = (get_documents_directory() / "CurrentStudy").string();
    } else {
        std::error_code ec;
        study_root = std::filesystem::absolute(std::filesystem::path(study_root), ec).string();
        if (ec) {
            return std::nullopt;
        }
    }

    if (legacy_template.empty()) {
        legacy_template = "sub-%p/ses-%s/%m/sub-%p_ses-%s_task-%b_run-%r_%m.xdf";
    }

    const bool use_run_r = !had_explicit_path_template && legacy_was_empty_initial;

    Defaults d;
    {
        constexpr const char kPrefix[] = "SessionBlocks\\";
        std::vector<std::pair<int, std::string>> indexed_blocks;
        for (const auto& pr : kv) {
            const std::string& key = pr.first;
            if (key.size() > sizeof(kPrefix) - 1 && key.compare(0, sizeof(kPrefix) - 1, kPrefix) == 0) {
                const int idx = std::atoi(key.c_str() + (sizeof(kPrefix) - 1));
                if (idx > 0) {
                    indexed_blocks.emplace_back(idx, pr.second);
                }
            }
        }
        if (!indexed_blocks.empty()) {
            std::sort(indexed_blocks.begin(), indexed_blocks.end(),
                [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) { return a.first < b.first; });
            d.block = indexed_blocks.front().second;
        } else {
            auto it_blocks = kv.find("SessionBlocks");
            if (it_blocks != kv.end()) {
                const auto blocks = parse_session_blocks_value(it_blocks->second);
                if (!blocks.empty()) {
                    d.block = blocks.front();
                }
            }
        }
    }

    const auto now_tp = now_for_placeholders();

    auto expand_one = [&](int counter) {
        std::string t = legacy_template;
        replace_filename_in_place(t, d, counter, use_run_r, now_tp);
        return t;
    };

    if (!template_has_counter(legacy_template, use_run_r)) {
        std::string expanded = expand_one(1);
        std::filesystem::path full = normalize_join(std::filesystem::path(study_root), expanded);
        std::error_code ec;
        std::filesystem::create_directories(full.parent_path(), ec);
        if (ec) {
            return std::nullopt;
        }
        const auto abs_full = std::filesystem::absolute(full);
        rename_existing_file_labrecorder_style(abs_full);
        return abs_full;
    }

    for (int i = 1; i <= 1000; ++i) {
        std::string expanded = expand_one(i);
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
