#pragma once
#include <windows.h>

#include <functional>

namespace ffapp {

enum class HotkeyAction { ToggleScaling, ToggleStats };

// Hidden message-only window that owns the global hotkeys (Ctrl+Alt+S, Ctrl+Alt+O).
class Hotkeys {
public:
    explicit Hotkeys(std::function<void(HotkeyAction)> callback);
    ~Hotkeys();
    Hotkeys(const Hotkeys&) = delete;
    Hotkeys& operator=(const Hotkeys&) = delete;

    bool ok() const noexcept { return m_hwnd != nullptr; }
    HWND hwnd() const noexcept { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    std::function<void(HotkeyAction)> m_callback;
    HWND m_hwnd = nullptr;
};

} // namespace ffapp
