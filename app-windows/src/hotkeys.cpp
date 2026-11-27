#include "hotkeys.hpp"

#include "log.hpp"

namespace ffapp {

namespace {
constexpr int kIdToggleScaling = 1;
constexpr int kIdToggleStats = 2;
constexpr int kIdQuit = 3;
constexpr int kIdCycleAlgo = 4;
constexpr int kIdSharpUp = 5;
constexpr int kIdSharpDown = 6;
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
    if (!RegisterHotKey(m_hwnd, kIdQuit, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q'))
        Log::write("RegisterHotKey Ctrl+Alt+Q failed (error %lu)", GetLastError());
    if (!RegisterHotKey(m_hwnd, kIdCycleAlgo, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'A'))
        Log::write("RegisterHotKey Ctrl+Alt+A failed (error %lu)", GetLastError());
    if (!RegisterHotKey(m_hwnd, kIdSharpUp, MOD_CONTROL | MOD_ALT, VK_UP))
        Log::write("RegisterHotKey Ctrl+Alt+Up failed (error %lu)", GetLastError());
    if (!RegisterHotKey(m_hwnd, kIdSharpDown, MOD_CONTROL | MOD_ALT, VK_DOWN))
        Log::write("RegisterHotKey Ctrl+Alt+Down failed (error %lu)", GetLastError());
}

Hotkeys::~Hotkeys() {
    if (m_hwnd) {
        UnregisterHotKey(m_hwnd, kIdToggleScaling);
        UnregisterHotKey(m_hwnd, kIdToggleStats);
        UnregisterHotKey(m_hwnd, kIdQuit);
        UnregisterHotKey(m_hwnd, kIdCycleAlgo);
        UnregisterHotKey(m_hwnd, kIdSharpUp);
        UnregisterHotKey(m_hwnd, kIdSharpDown);
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
            else if (wp == kIdQuit) self->m_callback(HotkeyAction::Quit);
            else if (wp == kIdCycleAlgo) self->m_callback(HotkeyAction::CycleAlgorithm);
            else if (wp == kIdSharpUp) self->m_callback(HotkeyAction::SharpnessUp);
            else if (wp == kIdSharpDown) self->m_callback(HotkeyAction::SharpnessDown);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace ffapp
