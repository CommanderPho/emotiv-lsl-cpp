#include "emotiv_lsl_log_config.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#else
#include <climits>
#include <unistd.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

static std::string exe_parent_dir() {
    namespace fs = std::filesystem;
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (GetModuleFileNameA(nullptr, buf, MAX_PATH) == 0) return {};
    return fs::path(buf).parent_path().string();
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::path(buf), ec);
    if (ec) return {};
    return p.parent_path().string();
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return fs::path(buf).parent_path().string();
#endif
}

void emotiv_set_lslapicfg_from_exe_dir() {
    if (std::getenv("LSLAPICFG")) return;
    std::string dir = exe_parent_dir();
    if (dir.empty()) return;
    namespace fs = std::filesystem;
    fs::path cfg = fs::path(dir) / "lsl_api.cfg";
    std::error_code ec;
    if (!fs::is_regular_file(cfg, ec)) return;
#if defined(_WIN32)
    _putenv_s("LSLAPICFG", cfg.string().c_str());
#else
    setenv("LSLAPICFG", cfg.string().c_str(), 0);
#endif
}
