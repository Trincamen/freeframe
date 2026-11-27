#pragma once
#include <windows.h>

#include <memory>
#include <string>

#include "freeframe/d3d11/scaler.hpp"
#include "hotkeys.hpp"

namespace ffapp {

struct Options {
    ff::d3d11::ScalerSettings scaler;
    std::wstring targetTitle;   // non-empty: start scaling the first visible window whose title contains it
    double exitAfterSeconds = 0; // 0 = run until quit
    std::wstring logPath;
    bool vsync = true;
    bool presentEveryVblank = false; // old behavior: re-render every swapchain vblank even without a new frame
};

// Application state machine: Idle <-> Scaling. Everything runs on the main thread.
class App {
public:
    explicit App(Options options);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Runs the message/render loop. Returns the process exit code.
    int run();

private:
    struct Session;

    void onHotkey(HotkeyAction action);
    bool startScaling(HWND target);
    void stopScaling(const char* reason);
    void renderFrame();
    void periodicChecks();
    void flushStats();

    Options m_options;
    std::unique_ptr<Hotkeys> m_hotkeys;
    std::unique_ptr<Session> m_session; // null == Idle
    bool m_showStats = true;
    bool m_titlePending = false;
    int m_exitCode = 0;
    long long m_startTicks = 0;
    long long m_lastCheckTicks = 0;
};

} // namespace ffapp
