#include "hotkeys.hpp"

#include "log.hpp"

namespace ffapp {

namespace {
constexpr int kIdToggleScaling = 1;
constexpr int kIdToggleStats = 2;
constexpr wchar_t kClass[] = L"FreeFrameMessageWindow";
} // namespace

Hotkeys::Hotkeys(std::function<void(HotkeyAction)> callback) : m_callback(std::move(callback)) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = &Hotkeys::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    m_hwnd = CreateWindowExW(0, kClass, L"FreeFrame", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, this);
    if (!m_hwnd) {
        Log::write("Failed to create message window (error %lu)", GetLastError());
        return;
    }
    if (!RegisterHotKey(m_hwnd, kIdToggleScaling, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'S'))
        Log::write("RegisterHotKey Ctrl+Alt+S failed (error %lu)", GetLastError());
    if (!RegisterHotKey(m_hwnd, kIdToggleStats, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O'))
        Log::write("RegisterHotKey Ctrl+Alt+O failed (error %lu)", GetLastError());
}

Hotkeys::~Hotkeys() {
    if (m_hwnd) {
        UnregisterHotKey(m_hwnd, kIdToggleScaling);
        UnregisterHotKey(m_hwnd, kIdToggleStats);
        DestroyWindow(m_hwnd);
    }
    UnregisterClassW(kClass, GetModuleHandleW(nullptr));
}

LRESULT CALLBACK Hotkeys::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    } else if (msg == WM_HOTKEY) {
        auto* self = reinterpret_cast<Hotkeys*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && self->m_callback) {
            if (wp == kIdToggleScaling) self->m_callback(HotkeyAction::ToggleScaling);
            else if (wp == kIdToggleStats) self->m_callback(HotkeyAction::ToggleStats);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace ffapp
