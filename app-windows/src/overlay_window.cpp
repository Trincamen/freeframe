#include "overlay_window.hpp"

#include "log.hpp"

namespace ffapp {

namespace {
constexpr wchar_t kClass[] = L"FreeFrameOverlay";

LRESULT CALLBACK overlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_ERASEBKGND: return 1;
    default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
} // namespace

OverlayWindow* OverlayWindow::create(const RECT& monitor) {
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = &overlayProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc); // already-registered is fine

    const DWORD ex = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    HWND hwnd = CreateWindowExW(ex, kClass, L"FreeFrame Overlay", WS_POPUP, monitor.left, monitor.top,
                                monitor.right - monitor.left, monitor.bottom - monitor.top, nullptr, nullptr, inst,
                                nullptr);
    if (!hwnd) {
        Log::write("CreateWindowEx(overlay) failed (error %lu)", GetLastError());
        return nullptr;
    }
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE))
        Log::write("SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE) failed (error %lu)", GetLastError());
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    auto* w = new OverlayWindow();
    w->m_hwnd = hwnd;
    w->m_rect = monitor;
    return w;
}

OverlayWindow::~OverlayWindow() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

} // namespace ffapp
