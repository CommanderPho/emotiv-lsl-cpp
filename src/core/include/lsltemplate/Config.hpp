#pragma once
/**
 * @file Config.hpp
 * @brief Configuration management for LSL applications
 *
 * Provides platform-independent configuration loading and saving.
 */

#include <filesystem>
#include <optional>
#include <string>

namespace lsltemplate {

/**
 * @brief Application configuration
 *
 * Customize this struct for your application's settings.
 */
struct AppConfig {
    std::string stream_name = "LSLTemplate";
    std::string stream_type = "Counter";
    int channel_count = 1;
    double sample_rate = 10.0;
    int device_param = 0;  // Device-specific parameter

    /// When true and no CLI --record path, emotiv_lsl resolves an XDF path from filename_template.
    bool recording_auto_enabled = false;
    /// Subdirectory under the executable directory when relative; absolute paths are used as-is.
    std::string recording_directory = "recordings";
    std::string recording_filename_template = "{stream_name}_{date}_{time}.xdf";
    /// If non-empty, used for {basename} placeholder; if empty, {basename} falls back to stream_name.
    std::string recording_basename = "";
};

/**
 * @brief Configuration file manager
 */
class ConfigManager {
public:
    /**
     * @brief Load configuration from file
     * @param path Path to config file
     * @return Loaded config, or nullopt on error
     */
    static std::optional<AppConfig> load(const std::filesystem::path& path);

    /**
     * @brief Save configuration to file
     * @param config Configuration to save
     * @param path Path to config file
     * @return true on success
     */
    static bool save(const AppConfig& config, const std::filesystem::path& path);

    /**
     * @brief Find config file in standard locations
     * @param filename Config filename (e.g., "MyApp.cfg")
     * @param hint Optional hint path to check first
     * @return Path to found config, or empty path if not found
     *
     * Search order:
     * 1. Hint path (if provided)
     * 2. Current working directory
     * 3. Executable directory
     * 4. Platform config directory (e.g., ~/.config on Linux)
     */
    static std::filesystem::path findConfigFile(
        const std::string& filename,
        const std::optional<std::filesystem::path>& hint = std::nullopt
    );

    /** Directory containing the running executable (empty if unknown). */
    static std::filesystem::path executableDirectory();

    /**
     * Builds a concrete .xdf path when recording_auto_enabled is set in config.
     * Creates parent directories as needed. Returns nullopt if disabled or on failure.
     */
    static std::optional<std::filesystem::path> resolveRecordingOutputPath(
        const AppConfig& config, const std::filesystem::path& exe_dir);
};

} // namespace lsltemplate
