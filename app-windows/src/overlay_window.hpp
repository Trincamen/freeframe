#pragma once
#include <windows.h>

namespace ffapp {

// Borderless, topmost, click-through, non-activating popup covering one monitor and excluded from capture.
class OverlayWindow {
public:
    // `monitor` is the monitor rectangle in virtual-screen pixels. Returns nullptr on failure.
    static OverlayWindow* create(const RECT& monitor);
    ~OverlayWindow();
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    HWND hwnd() const noexcept { return m_hwnd; }
    UINT width() const noexcept { return static_cast<UINT>(m_rect.right - m_rect.left); }
    UINT height() const noexcept { return static_cast<UINT>(m_rect.bottom - m_rect.top); }

private:
    OverlayWindow() = default;
    HWND m_hwnd = nullptr;
    RECT m_rect{};
};

} // namespace ffapp
