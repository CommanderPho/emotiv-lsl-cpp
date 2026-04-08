#pragma once

#include <filesystem>
#include <optional>
#include <string>

/// Discover `LabRecorder.cfg` using the same search order as App-LabRecorder (plus explicit path).
std::optional<std::filesystem::path> find_lab_recorder_config_file(
    const std::optional<std::filesystem::path>& explicit_path,
    const std::filesystem::path& emotiv_exe_dir);

/// Parse StudyRoot / PathTemplate / StorageLocation and resolve a concrete absolute .xdf path.
/// Returns nullopt if the file cannot be resolved (missing data, parse error, or BIDS-only without template handling).
std::optional<std::filesystem::path> resolve_lab_recorder_output_path(
    const std::filesystem::path& labrec_cfg_path);
