#include "app.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "capture_wgc.hpp"
#include "log.hpp"
#include "overlay_window.hpp"
#include "stats_overlay.hpp"
#include "swapchain.hpp"

namespace ffapp {

namespace {

long long nowTicks() {
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return v.QuadPart;
}
double ticksToSeconds(long long t) {
    static const double freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart);
    }();
    return static_cast<double>(t) / freq;
}

const char* algoName(ff::d3d11::ScaleAlgorithm a) {
    switch (a) {
    case ff::d3d11::ScaleAlgorithm::Nearest: return "nearest";
    case ff::d3d11::ScaleAlgorithm::Bilinear: return "bilinear";
    case ff::d3d11::ScaleAlgorithm::Fsr1: return "fsr1";
    }
    return "?";
}

bool isOwnWindow(HWND h) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

bool isCloaked(HWND h) {
    BOOL cloaked = FALSE;
    return SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked;
}

bool isCapturable(HWND h) {
    return h && IsWindow(h) && IsWindowVisible(h) && !IsIconic(h) && !isOwnWindow(h) && !isCloaked(h);
}

struct TitleSearch {
    std::wstring needle; // lower-case
    HWND found = nullptr;
};

std::wstring lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(s.data(), static_cast<DWORD>(s.size()));
    return s;
}

BOOL CALLBACK enumTitle(HWND h, LPARAM lp) {
    auto* s = reinterpret_cast<TitleSearch*>(lp);
    if (!isCapturable(h)) return TRUE;
    wchar_t title[512];
    const int n = GetWindowTextW(h, title, 512);
    if (n <= 0) return TRUE;
    if (lower(std::wstring(title, static_cast<size_t>(n))).find(s->needle) != std::wstring::npos) {
        s->found = h;
        return FALSE;
    }
    return TRUE;
}

HWND findWindowByTitle(const std::wstring& needle) {
    TitleSearch s{lower(needle), nullptr};
    EnumWindows(&enumTitle, reinterpret_cast<LPARAM>(&s));
    return s.found;
}

ff::d3d11::Rect aspectFit(ff::d3d11::Rect src, uint32_t dstW, uint32_t dstH) {
    if (src.width == 0 || src.height == 0 || dstW == 0 || dstH == 0) return {};
    const double scale = std::min(static_cast<double>(dstW) / src.width, static_cast<double>(dstH) / src.height);
    const auto w = std::min<uint32_t>(dstW, static_cast<uint32_t>(std::lround(src.width * scale)));
    const auto h = std::min<uint32_t>(dstH, static_cast<uint32_t>(std::lround(src.height * scale)));
    return {(dstW - w) / 2, (dstH - h) / 2, w, h};
}

} // namespace

// All per-scaling-session GPU/window resources. Members are destroyed in reverse order of declaration.
struct App::Session {
    HWND target = nullptr;
    ff::d3d11::Device dev;
    std::unique_ptr<CaptureWgc> capture;
    std::unique_ptr<OverlayWindow> overlay;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<ff::d3d11::Scaler> scaler;       
    std::unique_ptr<ff::d3d11::GpuTimer> gpuTimer;   // null -> CPU times only
    std::unique_ptr<StatsOverlay> stats;

    // Stats window (1 s)
    long long windowStart = 0;
    uint32_t baseFrames = 0, outFrames = 0;
    bool renderPending = false; // a new frame (or a stats refresh) is waiting for the swapchain waitable
    long long lastRenderTicks = 0;
    double cpuSeconds = 0;
    StatsSnapshot snapshot;

    // Last source/output pair written to the log (detects scaling changes).
    bool scaleLogged = false;
    uint32_t lastSrcW = 0, lastSrcH = 0, lastDstW = 0, lastDstH = 0;

    ~Session() {
        stats.reset();
        gpuTimer.reset();
        scaler.reset();
        swapchain.reset();
        overlay.reset();
        capture.reset();
        if (dev.context) {
            dev.context->ClearState();
            dev.context->Flush();
        }
    }
};

App::App(Options options) : m_options(std::move(options)) {
    m_startTicks = m_lastCheckTicks = nowTicks();
    m_hotkeys = std::make_unique<Hotkeys>([this](HotkeyAction a) { onHotkey(a); });
    if (!m_hotkeys->ok()) throw std::runtime_error("cannot create message window");
}

App::~App() { stopScaling("shutdown"); }

void App::onHotkey(HotkeyAction action) {
    if (action == HotkeyAction::ToggleStats) {
        m_showStats = !m_showStats;
        Log::write("Statistics overlay %s", m_showStats ? "on" : "off");
        return;
    }
    if (m_session) {
        stopScaling("hotkey");
        return;
    }
    HWND fg = GetForegroundWindow();
    if (!isCapturable(fg)) {
        Log::write("Hotkey ignored: foreground window is not capturable");
        return;
    }
    startScaling(fg);
}

bool App::startScaling(HWND target) {
    auto s = std::make_unique<Session>();
    s->target = target;
    wchar_t title[256] = L"";
    GetWindowTextW(target, title, 256);
    Log::write("Start scaling hwnd=%p title=\"%s\" algo=%s sharpness=%.2f", static_cast<void*>(target),
               toUtf8(title).c_str(), algoName(m_options.scaler.algorithm), m_options.scaler.sharpness);

    auto dev = ff::d3d11::createDevice();
    if (!dev.ok()) {
        Log::write("Cannot start: D3D11 device creation failed: %s", dev.error().c_str());
        return false;
    }
    s->dev = std::move(dev.value());
    ID3D11Device* device = s->dev.device.Get();
    ID3D11DeviceContext* ctx = s->dev.context.Get();

    auto sc = ff::d3d11::Scaler::create(device);
    if (!sc.ok()) {
        Log::write("Cannot start: Scaler::create failed: %s", sc.error().c_str());
        return false;
    }
    s->scaler = std::move(sc.value());
    auto timer = ff::d3d11::GpuTimer::create(device);
    if (timer.ok()) s->gpuTimer = std::move(timer.value());
    else Log::write("GpuTimer::create failed (%s); CPU times only", timer.error().c_str());

    std::string err;
    s->capture = CaptureWgc::create(device, target, err);
    if (!s->capture) {
        Log::write("Capture failed: %s", err.c_str());
        return false;
    }

    const HMONITOR mon = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(mon, &mi)) {
        Log::write("GetMonitorInfo failed");
        return false;
    }
    s->overlay.reset(OverlayWindow::create(mi.rcMonitor));
    if (!s->overlay) return false;

    Swapchain::Config cfg;
    cfg.width = s->overlay->width();
    cfg.height = s->overlay->height();
    cfg.unorderedAccess = true;
    cfg.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    s->swapchain = Swapchain::create(device, s->overlay->hwnd(), cfg, err);
    if (!s->swapchain) {
        Log::write("Swapchain failed: %s", err.c_str());
        return false;
    }
    Log::write("Overlay %ux%u, tearing %s, vsync %s", cfg.width, cfg.height,
               s->swapchain->tearingAvailable() ? "available" : "unavailable", m_options.vsync ? "on" : "off");

    s->stats = std::make_unique<StatsOverlay>();
    if (!s->stats->init(s->overlay->hwnd(), device, ctx)) {
        Log::write("ImGui init failed; statistics overlay disabled");
        s->stats.reset();
    }
    s->snapshot.algorithm = algoName(m_options.scaler.algorithm);
    s->windowStart = nowTicks();
    m_session = std::move(s);
    return true;
}

void App::stopScaling(const char* reason) {
    if (!m_session) return;
    Log::write("Stop scaling (%s)", reason);
    m_session.reset();
}

void App::renderFrame() {
    Session& s = *m_session;
    const long long t0 = nowTicks();
    ID3D11DeviceContext* ctx = s.dev.context.Get();
    auto* timer = s.gpuTimer.get();

    if (timer) timer->beginFrame(ctx);

    if (timer) timer->beginStage(ctx, ff::d3d11::Stage::Capture);
    if (s.capture->update(ctx)) ++s.baseFrames;
    if (timer) timer->endStage(ctx, ff::d3d11::Stage::Capture);

    const float black[4] = {0, 0, 0, 1};
    ctx->ClearRenderTargetView(s.swapchain->rtv(), black);

    if (s.capture->hasFrame()) {
        const ff::d3d11::Rect src = s.capture->clientRect();
        const ff::d3d11::Rect dst = aspectFit(src, s.swapchain->width(), s.swapchain->height());
        if (src.width && dst.width) {
            const bool changed = !s.scaleLogged || src.width != s.lastSrcW || src.height != s.lastSrcH ||
                                 dst.width != s.lastDstW || dst.height != s.lastDstH;
            if (changed) {
                s.scaleLogged = true;
                s.lastSrcW = src.width;
                s.lastSrcH = src.height;
                s.lastDstW = dst.width;
                s.lastDstH = dst.height;
                s.snapshot.srcW = src.width;
                s.snapshot.srcH = src.height;
                s.snapshot.dstW = dst.width;
                s.snapshot.dstH = dst.height;
                const double factor = static_cast<double>(dst.width) / static_cast<double>(src.width);
                Log::write("Scaling %ux%u -> %ux%u (%s, factor %.2fx)", src.width, src.height, dst.width, dst.height,
                           algoName(m_options.scaler.algorithm), factor);
                if (dst.width == src.width && dst.height == src.height)
                    Log::write("Note: source already at output resolution - no upscaling. Run the game windowed at a "
                               "lower resolution (e.g. 1280x720).");
            }
            if (timer) timer->beginStage(ctx, ff::d3d11::Stage::Scale);
            auto r = s.scaler->run(ctx, s.capture->srv(), src, s.swapchain->uav(), dst, m_options.scaler);
            if (!r.ok()) Log::write("Scaler::run failed: %s", r.error().c_str());
            if (timer) timer->endStage(ctx, ff::d3d11::Stage::Scale);
        }
    }

    if (m_showStats && s.stats) {
        if (timer) timer->beginStage(ctx, ff::d3d11::Stage::Overlay);
        s.stats->render(s.swapchain->rtv(), s.swapchain->width(), s.swapchain->height(), s.snapshot);
        if (timer) timer->endStage(ctx, ff::d3d11::Stage::Overlay);
    }
    if (timer) timer->endFrame(ctx);

    s.cpuSeconds += ticksToSeconds(nowTicks() - t0);
    s.lastRenderTicks = t0;

    const HRESULT hr = s.swapchain->present(m_options.vsync);
    ++s.outFrames;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        Log::write("Device removed/reset (Present 0x%08lX, reason 0x%08lX)", static_cast<unsigned long>(hr),
                   static_cast<unsigned long>(s.dev.device->GetDeviceRemovedReason()));
        stopScaling("device removed");
        return;
    }
    if (FAILED(hr)) Log::write("Present failed 0x%08lX", static_cast<unsigned long>(hr));
}

void App::flushStats() {
    Session& s = *m_session;
    auto* timer = s.gpuTimer.get();
    const double elapsed = ticksToSeconds(nowTicks() - s.windowStart);
    if (elapsed >= 1.0) {
        StatsSnapshot& sn = s.snapshot;
        sn.baseFps = s.baseFrames / elapsed;
        sn.outFps = s.outFrames / elapsed;
        sn.cpuMs = s.outFrames ? s.cpuSeconds * 1000.0 / s.outFrames : 0.0;
        sn.gpuValid = timer != nullptr;
        if (timer) {
            const auto g = timer->lastStageMs();
            sn.gpuMs = {g[0], g[1], g[2]};
        }
        if (sn.gpuValid)
            Log::write("base %.1f fps | out %.1f fps | cpu %.2f ms | gpu capture %.2f scale %.2f overlay %.2f ms",
                       sn.baseFps, sn.outFps, sn.cpuMs, sn.gpuMs[0], sn.gpuMs[1], sn.gpuMs[2]);
        else
            Log::write("base %.1f fps | out %.1f fps | cpu %.2f ms | gpu n/a", sn.baseFps, sn.outFps, sn.cpuMs);
        s.windowStart = nowTicks();
        s.baseFrames = s.outFrames = 0;
        s.cpuSeconds = 0;
    }
}

void App::periodicChecks() {
    const long long now = nowTicks();
    if (ticksToSeconds(now - m_lastCheckTicks) < 0.1) return;
    m_lastCheckTicks = now;

    if (m_options.exitAfterSeconds > 0 && ticksToSeconds(now - m_startTicks) >= m_options.exitAfterSeconds) {
        Log::write("--exit-after reached");
        PostQuitMessage(m_exitCode);
        return;
    }
    if (m_session) {
        const HWND t = m_session->target;
        if (!IsWindow(t)) stopScaling("target closed");
        else if (IsIconic(t)) stopScaling("target minimized");
        else if (m_session->capture->closed()) stopScaling("capture item closed");
        else flushStats();
    } else if (m_titlePending) {
        if (HWND h = findWindowByTitle(m_options.targetTitle)) {
            m_titlePending = false;
            if (!startScaling(h)) {
                stopScaling("start failed");
                m_exitCode = 1;
                PostQuitMessage(m_exitCode);
            }
        }
    }
}

int App::run() {
    Log::write("FreeFrame started (Ctrl+Alt+S: toggle scaling, Ctrl+Alt+O: toggle statistics)");
    m_titlePending = !m_options.targetTitle.empty();
    if (m_titlePending && m_options.exitAfterSeconds <= 0)
        Log::write("Waiting for a window titled \"%s\"", toUtf8(m_options.targetTitle).c_str());

    bool quit = false;
    while (!quit) {
        // Wait for a new captured frame first, then for the swapchain waitable before rendering.
        HANDLE wait = nullptr;
        if (m_session) {
            const bool waitSwapchain = m_options.presentEveryVblank || m_session->renderPending;
            wait = waitSwapchain ? m_session->swapchain->waitable() : m_session->capture->frameEvent();
        }
        const DWORD r = MsgWaitForMultipleObjectsEx(wait ? 1u : 0u, wait ? &wait : nullptr, 100, QS_ALLINPUT,
                                                    MWMO_INPUTAVAILABLE);

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) break;

        if (r == WAIT_OBJECT_0 && m_session) {
            if (m_options.presentEveryVblank || m_session->renderPending) {
                m_session->renderPending = false;
                try {
                    renderFrame();
                } catch (const std::exception& e) {
                    Log::write("Render error: %s", e.what());
                    stopScaling("error");
                }
            } else {
                m_session->renderPending = true; // new frame arrived; now wait for the swapchain
            }
        }
        // Keep the statistics text alive (~10 Hz) even if the source is static.
        if (m_session && m_showStats && !m_session->renderPending &&
            ticksToSeconds(nowTicks() - m_session->lastRenderTicks) >= 0.1)
            m_session->renderPending = true;
        periodicChecks();
    }
    stopScaling("exit");
    if (m_titlePending) {
        Log::write("Target window never found");
        m_exitCode = 1;
    }
    return m_exitCode;
}

} // namespace ffapp
