// FreeFrame test game: windowed D3D11 app with a fixed test pattern, input/focus logging.
// Test tool only; not part of the release.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <xinput.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <share.h>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

int g_w = 1280, g_h = 720;
HWND g_hwnd = nullptr;
FILE* g_log = nullptr;
std::chrono::steady_clock::time_point g_t0;
bool g_active = true;
bool g_pauseOnInactive = false;
ULONGLONG g_lastMoveLog = 0;

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_t0).count();
}

void logf(const char* fmt, ...) {
    if (!g_log) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(g_log, "[%8lld] %s\n", nowMs(), buf);
    fflush(g_log);
}

int sx(LPARAM l) { return static_cast<short>(LOWORD(l)); }
int sy(LPARAM l) { return static_cast<short>(HIWORD(l)); }

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ACTIVATEAPP:
        logf("ACTIVATE %d (app)", w ? 1 : 0);
        g_active = w != 0;
        return 0;
    case WM_ACTIVATE: logf("ACTIVATE %d (wnd)", LOWORD(w) != WA_INACTIVE ? 1 : 0); return 0;
    case WM_SETFOCUS: logf("FOCUS gained"); return 0;
    case WM_KILLFOCUS: logf("FOCUS lost"); return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!(l & (1 << 30))) logf("KEY down vk=0x%02X", static_cast<unsigned>(w));
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP: logf("KEY up vk=0x%02X", static_cast<unsigned>(w)); return 0;
    case WM_LBUTTONDOWN: logf("MOUSE ldown %d,%d", sx(l), sy(l)); return 0;
    case WM_LBUTTONUP: logf("MOUSE lup %d,%d", sx(l), sy(l)); return 0;
    case WM_RBUTTONDOWN: logf("MOUSE rdown %d,%d", sx(l), sy(l)); return 0;
    case WM_MOUSEMOVE: {
        const ULONGLONG t = GetTickCount64();
        if (t - g_lastMoveLog >= 100) {
            g_lastMoveLog = t;
            logf("MOUSEMOVE %d,%d", sx(l), sy(l));
        }
        return 0;
    }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---- CPU pattern -----------------------------------------------------------------------------
using Pixels = std::vector<uint32_t>;
constexpr uint32_t bgra(unsigned r, unsigned g, unsigned b) { return 0xFF000000u | (r << 16) | (g << 8) | b; }
size_t idx(int x, int y) { return static_cast<size_t>(y) * static_cast<size_t>(g_w) + static_cast<size_t>(x); }

Pixels makeBackground() {
    Pixels p(static_cast<size_t>(g_w) * static_cast<size_t>(g_h));
    for (int y = 0; y < g_h; ++y)
        for (int x = 0; x < g_w; ++x) {
            uint32_t c = bgra(24, 24, 28);
            if (y < 120) {
                const unsigned v = static_cast<unsigned>(x * 255 / (g_w - 1));
                c = y < 40 ? bgra(v, v, v) : y < 80 ? bgra(v, 0, 255 - v) : bgra(0, v, 128);
            } else if (y < 220 && x < g_w / 2) {
                c = ((x ^ y) & 1) ? bgra(255, 255, 255) : bgra(0, 0, 0);
            } else if (y < 220) {
                c = (((x >> 1) ^ (y >> 1)) & 1) ? bgra(230, 230, 60) : bgra(20, 20, 90);
            } else if (y >= g_h - 120) {
                const unsigned v = static_cast<unsigned>((y - (g_h - 120)) * 255 / 119);
                c = bgra(v, 255 - v, v / 2);
            } else if (x % 16 == 0 || y % 16 == 0) {
                c = bgra(90, 200, 90);
            }
            p[idx(x, y)] = c;
        }
    for (int x = 0; x < g_w; ++x) {
        p[idx(x, 0)] = bgra(255, 0, 0);
        p[idx(x, g_h - 1)] = bgra(0, 255, 0);
    }
    for (int y = 0; y < g_h; ++y) {
        p[idx(0, y)] = bgra(0, 0, 255);
        p[idx(g_w - 1, y)] = bgra(255, 255, 0);
    }
    return p;
}

void fillRect(Pixels& p, int x0, int y0, int w, int h, uint32_t c) {
    for (int y = std::max(y0, 0); y < std::min(y0 + h, g_h); ++y)
        for (int x = std::max(x0, 0); x < std::min(x0 + w, g_w); ++x) p[idx(x, y)] = c;
}

void drawFrame(Pixels& p, const Pixels& bg, unsigned frame) {
    p = bg;
    const int sz = 64, range = g_w - 2 - sz;
    const int t = static_cast<int>(frame * 6u % static_cast<unsigned>(range * 2));
    const int x = 1 + (t < range ? t : range * 2 - t);
    fillRect(p, x, 300, sz, sz, bgra(255, 128, 0));
    fillRect(p, x + 4, 304, sz - 8, sz - 8, bgra(255, 255, 255));
    // frame counter as 24 binary boxes (MSB left)
    for (int b = 0; b < 24; ++b) {
        const bool on = ((frame >> (23 - b)) & 1u) != 0;
        fillRect(p, 20 + b * 20, g_h - 100, 16, 16, on ? bgra(255, 255, 255) : bgra(40, 40, 40));
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_t0 = std::chrono::steady_clock::now();

    int posX = CW_USEDEFAULT, posY = CW_USEDEFAULT, fps = 60;
    double exitAfter = 0;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        const std::wstring a = argv[i];
        const bool hv = i + 1 < argc;
        if (a == L"--size" && hv) {
            (void)swscanf_s(argv[++i], L"%dx%d", &g_w, &g_h);
        } else if (a == L"--pos" && hv) {
            (void)swscanf_s(argv[++i], L"%d,%d", &posX, &posY);
        } else if (a == L"--fps" && hv) {
            fps = std::max(1, _wtoi(argv[++i]));
        } else if (a == L"--exit-after" && hv) {
            exitAfter = _wtof(argv[++i]);
        } else if (a == L"--log" && hv) {
            g_log = _wfsopen(argv[++i], L"w", _SH_DENYWR);
        } else if (a == L"--pause-on-inactive") {
            g_pauseOnInactive = true;
        }
    }
    if (argv) LocalFree(argv);
    g_w = std::max(g_w, 400);
    g_h = std::max(g_h, 400);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FreeFrameTestGame";
    RegisterClassExW(&wc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const UINT dpi = GetDpiForSystem();
    RECT r{0, 0, g_w, g_h};
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"FreeFrame Test Game", style, posX, posY, r.right - r.left,
                             r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 2;
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);

    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                 D3D11_SDK_VERSION, &dev, nullptr, &ctx)))
        return 3;
    ComPtr<IDXGIDevice> dxgiDev;
    dev.As(&dxgiDev);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDev->GetAdapter(&adapter);
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = static_cast<UINT>(g_w);
    sd.Height = static_cast<UINT>(g_h);
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> sc;
    if (FAILED(factory->CreateSwapChainForHwnd(dev.Get(), g_hwnd, &sd, nullptr, nullptr, &sc))) return 4;
    factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER);

    D3D11_TEXTURE2D_DESC td{};
    td.Width = sd.Width;
    td.Height = sd.Height;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex))) return 5;

    const Pixels bg = makeBackground();
    Pixels frameBuf;

    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    LARGE_INTEGER qpf, qpc;
    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&qpc);
    const long long period = qpf.QuadPart / fps;
    long long next = qpc.QuadPart + period;

    unsigned frame = 0, framesThisSec = 0;
    ULONGLONG lastStat = GetTickCount64(), lastPad = 0;
    WORD lastButtons = 0xFFFF;
    bool lastConnected = false, padInit = false;
    logf("START size=%dx%d fps=%d dpi=%u", g_w, g_h, fps, dpi);

    bool quit = false;
    while (!quit) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) quit = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) break;

        const ULONGLONG tick = GetTickCount64();
        if (exitAfter > 0 && nowMs() >= static_cast<long long>(exitAfter * 1000)) {
            logf("EXIT-AFTER");
            DestroyWindow(g_hwnd);
            continue;
        }

        if (tick - lastPad >= 100) { // polling an absent pad every frame is slow
            lastPad = tick;
            XINPUT_STATE xs{};
            const bool conn = XInputGetState(0, &xs) == ERROR_SUCCESS;
            if (!padInit || conn != lastConnected) {
                logf("XINPUT pad0 %s", conn ? "connected" : "disconnected");
                lastConnected = conn;
                padInit = true;
                lastButtons = 0xFFFF;
            }
            if (conn && xs.Gamepad.wButtons != lastButtons) {
                logf("XINPUT pad0 buttons=0x%04X", xs.Gamepad.wButtons);
                lastButtons = xs.Gamepad.wButtons;
            }
        }

        if (tick - lastStat >= 1000) {
            logf("STAT fps=%u foreground=%d", framesThisSec, GetForegroundWindow() == g_hwnd ? 1 : 0);
            framesThisSec = 0;
            lastStat += 1000;
        }

        if (!(g_pauseOnInactive && !g_active)) ++frame;
        drawFrame(frameBuf, bg, frame);
        ctx->UpdateSubresource(tex.Get(), 0, nullptr, frameBuf.data(), static_cast<UINT>(g_w) * 4u, 0);
        ComPtr<ID3D11Texture2D> back;
        sc->GetBuffer(0, IID_PPV_ARGS(&back));
        ctx->CopyResource(back.Get(), tex.Get());
        sc->Present(0, 0);
        ++framesThisSec;

        // precise limiter: high-res timer until ~1 ms before the deadline, then spin
        QueryPerformanceCounter(&qpc);
        const long long remaining = next - qpc.QuadPart;
        if (remaining > qpf.QuadPart / 500 && timer) {
            LARGE_INTEGER due;
            due.QuadPart = -((remaining - qpf.QuadPart / 1000) * 10000000LL / qpf.QuadPart);
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, 100);
        }
        do QueryPerformanceCounter(&qpc);
        while (qpc.QuadPart < next);
        next += period;
        if (qpc.QuadPart > next + period * 4) next = qpc.QuadPart + period;
    }
    logf("END frames=%u", frame);
    if (g_log) fclose(g_log);
    return 0;
}
