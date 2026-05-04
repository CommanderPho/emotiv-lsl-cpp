#include "lsltemplate/Config.hpp"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <linux/limits.h>
#endif
#endif

namespace lsltemplate {

namespace {

// Simple INI parser helpers
std::string trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(),
        [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(str.rbegin(), str.rend(),
        [](unsigned char c) { return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

std::filesystem::path getExecutablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::filesystem::path(buffer).parent_path();
    }
    return {};
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        return std::filesystem::path(buffer).parent_path();
    }
    return {};
#endif
}

std::filesystem::path getConfigDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buffer))) {
        return std::filesystem::path(buffer);
    }
    return {};
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / "Library" / "Preferences";
    }
    return {};
#else
    // XDG Base Directory Specification
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && *xdg_config) {
        return std::filesystem::path(xdg_config);
    }
    const char* home = getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config";
    }
    return {};
#endif
}

bool parse_bool_value(const std::string& v) {
    std::string t = trim(v);
    std::transform(t.begin(), t.end(), t.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return t == "1" || t == "true" || t == "yes" || t == "on";
}

void replace_placeholder(std::string& s, const std::string& key, const std::string& val) {
    const std::string ph = "{" + key + "}";
    for (size_t pos = 0; (pos = s.find(ph, pos)) != std::string::npos;) {
        s.replace(pos, ph.size(), val);
        pos += val.size();
    }
}

void sanitize_relative_path_string(std::string& s) {
    for (char& c : s) {
        if (c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
}

} // anonymous namespace

std::optional<AppConfig> ConfigManager::load(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    AppConfig config;
    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        // Key=value pair
        auto eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));

            // Remove quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            if (current_section == "Stream") {
                if (key == "name" || key == "stream_name") {
                    config.stream_name = value;
                } else if (key == "type" || key == "stream_type") {
                    config.stream_type = value;
                } else if (key == "channels" || key == "channel_count") {
                    config.channel_count = std::stoi(value);
                } else if (key == "sample_rate" || key == "srate") {
                    config.sample_rate = std::stod(value);
                }
            } else if (current_section == "Device") {
                if (key == "device" || key == "device_param") {
                    config.device_param = std::stoi(value);
                }
            } else if (current_section == "Recording") {
                if (key == "enabled") {
                    config.recording_auto_enabled = parse_bool_value(value);
                } else if (key == "directory") {
                    config.recording_directory = value;
                } else if (key == "filename_template") {
                    config.recording_filename_template = value;
                } else if (key == "basename") {
                    config.recording_basename = value;
                }
            }
        }
    }

    return config;
}

bool ConfigManager::save(const AppConfig& config, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "# LSL Application Configuration\n";
    file << "[Stream]\n";
    file << "name=" << config.stream_name << "\n";
    file << "type=" << config.stream_type << "\n";
    file << "channels=" << config.channel_count << "\n";
    file << "sample_rate=" << config.sample_rate << "\n";
    file << "\n";
    file << "[Device]\n";
    file << "device_param=" << config.device_param << "\n";
    file << "\n";
    file << "[Recording]\n";
    file << "enabled=" << (config.recording_auto_enabled ? 1 : 0) << "\n";
    file << "directory=" << config.recording_directory << "\n";
    file << "filename_template=" << config.recording_filename_template << "\n";
    file << "basename=" << config.recording_basename << "\n";

    return file.good();
}

std::filesystem::path ConfigManager::findConfigFile(
    const std::string& filename,
    const std::optional<std::filesystem::path>& hint
) {
    std::vector<std::filesystem::path> search_paths;

    // 1. Hint path
    if (hint && std::filesystem::exists(*hint)) {
        return *hint;
    }

    // 2. Current working directory
    search_paths.push_back(std::filesystem::current_path());

    // 3. Executable directory
    auto exe_path = getExecutablePath();
    if (!exe_path.empty()) {
        search_paths.push_back(exe_path);
    }

    // 4. Platform config directory
    auto config_dir = getConfigDirectory();
    if (!config_dir.empty()) {
        search_paths.push_back(config_dir);
    }

    // Search for the file
    for (const auto& dir : search_paths) {
        auto full_path = dir / filename;
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }
    }

    return {};
}

std::filesystem::path ConfigManager::executableDirectory() {
    return getExecutablePath();
}

std::optional<std::filesystem::path> ConfigManager::resolveRecordingOutputPath(
    const AppConfig& config, const std::filesystem::path& exe_dir) {
    if (!config.recording_auto_enabled) {
        return std::nullopt;
    }
    const std::filesystem::path exe =
        exe_dir.empty() ? std::filesystem::current_path() : exe_dir;
    std::filesystem::path dir_path(config.recording_directory);
    std::filesystem::path base = dir_path.is_absolute() ? dir_path : (exe / dir_path);

    std::string name_part = config.recording_filename_template;
    if (name_part.empty()) {
        name_part = "{stream_name}_{date}_{time}.xdf";
    }

    const std::string basename_val =
        config.recording_basename.empty() ? config.stream_name : config.recording_basename;

    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32)
    if (localtime_s(&local_tm, &tt) != 0) {
        return std::nullopt;
    }
#else
    if (localtime_r(&tt, &local_tm) == nullptr) {
        return std::nullopt;
    }
#endif
    char date_buf[32];
    char time_buf[32];
    if (std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &local_tm) == 0) {
        return std::nullopt;
    }
    if (std::strftime(time_buf, sizeof(time_buf), "%H-%M-%S", &local_tm) == 0) {
        return std::nullopt;
    }

    replace_placeholder(name_part, "stream_name", config.stream_name);
    replace_placeholder(name_part, "basename", basename_val);
    replace_placeholder(name_part, "date", std::string(date_buf));
    replace_placeholder(name_part, "time", std::string(time_buf));
    sanitize_relative_path_string(name_part);

    std::filesystem::path out = base / name_part;
    std::error_code ec;
    std::filesystem::create_directories(out.parent_path(), ec);
    if (ec) {
        return std::nullopt;
    }
    return std::filesystem::absolute(out);
}

} // namespace lsltemplate
