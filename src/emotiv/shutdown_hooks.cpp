#include "shutdown_hooks.h"
#include "emotiv_base.h"
#include <atomic>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#endif

namespace {
std::atomic<EmotivBase*> g_emotiv_for_shutdown{nullptr};

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD /*ctrl_type*/) {
    EmotivBase* p = g_emotiv_for_shutdown.load(std::memory_order_acquire);
    if (p) {
        p->requestShutdown();
    }
    return TRUE;
}
#else
void posix_signal_handler(int /*signum*/) {
    EmotivBase* p = g_emotiv_for_shutdown.load(std::memory_order_acquire);
    if (p) {
        p->requestShutdown();
    }
}
#endif
} // namespace

void install_emotiv_shutdown_handlers(EmotivBase* instance) {
    g_emotiv_for_shutdown.store(instance, std::memory_order_release);
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    std::signal(SIGINT, posix_signal_handler);
    std::signal(SIGTERM, posix_signal_handler);
#endif
}

void remove_emotiv_shutdown_handlers() {
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, FALSE);
#else
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
#endif
    g_emotiv_for_shutdown.store(nullptr, std::memory_order_release);
}
