#include "app.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

#include "capture_wgc.hpp"
#include "cursor_map.hpp"
#include "log.hpp"
#include "overlay_window.hpp"
#include "stats_overlay.hpp"
#include "strings.hpp"
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

const char* algoDisplayName(ff::d3d11::ScaleAlgorithm a) {
    switch (a) {
    case ff::d3d11::ScaleAlgorithm::Nearest: return str::kAlgoNearest;
    case ff::d3d11::ScaleAlgorithm::Bilinear: return str::kAlgoBilinear;
    case ff::d3d11::ScaleAlgorithm::Fsr1: return str::kAlgoFsr1;
    }
    return "?";
}

ff::AlgorithmKind algoKind(ff::d3d11::ScaleAlgorithm a) {
    switch (a) {
    case ff::d3d11::ScaleAlgorithm::Nearest: return ff::AlgorithmKind::Nearest;
    case ff::d3d11::ScaleAlgorithm::Bilinear: return ff::AlgorithmKind::Bilinear;
    case ff::d3d11::ScaleAlgorithm::Fsr1: return ff::AlgorithmKind::Fsr1;
    }
    return ff::AlgorithmKind::Other;
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
    std::unique_ptr<CursorCache> cursorCache;
    std::unique_ptr<CursorMapper> cursor; // null when --no-cursor-map
    // Last cursor state that was rendered (to detect movement).
    POINT lastCursorPos{-1, -1};
    HCURSOR lastCursor = nullptr;
    bool lastCursorShowing = false;
    HWND lastForeground = nullptr; // for diagnostics: logs every foreground change during the session
    long long startTicks = 0;
    // Transient notices
    long long toastSettingsUntil = 0, toastTipUntil = 0;
    std::string toastSettingsText, toastTipText;
    bool toastShown = false; // a toast was drawn by the last render (one more render is needed to clear it)
    // Tips
    std::set<std::string> shownTips;
    // Average base FPS per source height (windows of 1 s, fps > 0, after 3 s on that resolution)
    struct FpsAcc {
        double sum = 0;
        int count = 0;
    };
    std::map<uint32_t, FpsAcc> fpsByHeight;
    uint32_t curHeight = 0;
    long long curHeightSince = 0;
    bool paused = false; // target lost focus: overlay hidden, nothing rendered
    bool loggedFg1s = false;

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
        cursor.reset(); // restore clip + system cursor first
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
    recoverCursorStateIfNeeded();
    m_hotkeys = std::make_unique<Hotkeys>([this](HotkeyAction a) { onHotkey(a); });
    if (!m_hotkeys->ok()) throw std::runtime_error("cannot create message window");
}

App::~App() { stopScaling("shutdown"); }

void App::onHotkey(HotkeyAction action) {
    if (action == HotkeyAction::Quit) {
        Log::write("Quit hotkey pressed");
        stopScaling("quit");
        restoreCursorStateGlobal();
        PostQuitMessage(0);
        return;
    }
    if (action == HotkeyAction::CycleAlgorithm || action == HotkeyAction::SharpnessUp ||
        action == HotkeyAction::SharpnessDown) {
        auto& cfg = m_options.scaler;
        char toast[64];
        if (action == HotkeyAction::CycleAlgorithm) {
            using A = ff::d3d11::ScaleAlgorithm;
            cfg.algorithm = cfg.algorithm == A::Nearest ? A::Bilinear : (cfg.algorithm == A::Bilinear ? A::Fsr1 : A::Nearest);
            Log::write("Algorithm changed to %s", algoName(cfg.algorithm));
            snprintf(toast, sizeof toast, str::kToastAlgo, algoDisplayName(cfg.algorithm));
            if (m_session) m_session->snapshot.algorithm = algoName(cfg.algorithm);
        } else {
            const float delta = action == HotkeyAction::SharpnessUp ? 0.1f : -0.1f;
            cfg.sharpness = std::clamp(std::round((cfg.sharpness + delta) * 10.0f) / 10.0f, 0.0f, 1.0f);
            Log::write("Sharpness changed to %.1f", cfg.sharpness);
            snprintf(toast, sizeof toast, str::kToastSharp, cfg.sharpness);
        }
        if (m_session) {
            m_session->toastSettingsText = toast;
            m_session->toastSettingsUntil = nowTicks() + static_cast<long long>(2.5 / ticksToSeconds(1));
            m_session->renderPending = true;
        }
        return;
    }
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

    // Keyboard/mouse focus must stay on the target; the overlay never activates.
    if (GetForegroundWindow() != target) {
        Log::write("Foreground is not the target; calling SetForegroundWindow");
        SetForegroundWindow(target);
    }
    {
        wchar_t fgTitle[256] = L"";
        if (HWND fg = GetForegroundWindow()) GetWindowTextW(fg, fgTitle, 256);
        Log::write("foreground after start: %s", toUtf8(fgTitle).c_str());
    }
    s->lastForeground = GetForegroundWindow();
    s->startTicks = nowTicks();
    if (m_options.cursorMap) {
        s->cursorCache = std::make_unique<CursorCache>(device);
        s->cursor = std::make_unique<CursorMapper>();
        s->cursor->start(target);
    }
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

    CursorDraw cursorDraw;
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

            if (s.cursor && s.cursor->drawVirtualCursor()) {
                CURSORINFO ci{};
                ci.cbSize = sizeof ci;
                POINT origin{};
                if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING) && s.cursor->clientOrigin(origin)) {
                    const double cx = ci.ptScreenPos.x - origin.x, cy = ci.ptScreenPos.y - origin.y;
                    if (cx >= 0 && cy >= 0 && cx < src.width && cy < src.height) {
                        if (const CursorSprite* sp = s.cursorCache->get(ci.hCursor)) {
                            const double scale = static_cast<double>(dst.width) / src.width;
                            const double cs = m_options.cursorScale > 0.0f ? m_options.cursorScale : scale;
                            cursorDraw.srv = sp->srv;
                            cursorDraw.x = static_cast<float>(dst.x + cx * scale - sp->hotX * cs);
                            cursorDraw.y = static_cast<float>(dst.y + cy * scale - sp->hotY * cs);
                            cursorDraw.w = static_cast<float>(sp->width * cs);
                            cursorDraw.h = static_cast<float>(sp->height * cs);
                        }
                    }
                }
            }
        }
    }

    {
        const long long now = nowTicks();
        s.snapshot.toastSettings = now < s.toastSettingsUntil ? s.toastSettingsText : std::string();
        s.snapshot.toastTip = now < s.toastTipUntil ? s.toastTipText : std::string();
    }
    const bool toastNow = !s.snapshot.toastSettings.empty() || !s.snapshot.toastTip.empty();
    if (s.stats && (m_showStats || cursorDraw.srv || toastNow)) {
        if (timer) timer->beginStage(ctx, ff::d3d11::Stage::Overlay);
        s.stats->render(s.swapchain->rtv(), s.swapchain->width(), s.swapchain->height(), s.snapshot, m_showStats,
                        cursorDraw.srv ? &cursorDraw : nullptr);
        if (timer) timer->endStage(ctx, ff::d3d11::Stage::Overlay);
    }
    s.toastShown = toastNow;
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

void App::updateTips(StatsSnapshot& sn) {
    Session& s = *m_session;
    if (sn.srcW == 0 || sn.srcH == 0) return;
    const long long now = nowTicks();
    if (sn.srcH != s.curHeight) {
        s.curHeight = sn.srcH;
        s.curHeightSince = now;
    } else if (sn.baseFps > 0.0 && ticksToSeconds(now - s.curHeightSince) >= 3.0) {
        auto& acc = s.fpsByHeight[sn.srcH];
        acc.sum += sn.baseFps;
        ++acc.count;
    }

    ff::SessionContext ctx;
    ctx.srcWidth = sn.srcW;
    ctx.srcHeight = sn.srcH;
    ctx.dstWidth = sn.dstW;
    ctx.dstHeight = sn.dstH;
    ctx.monitorWidth = s.swapchain->width();
    ctx.monitorHeight = s.swapchain->height();
    ctx.algorithm = algoKind(m_options.scaler.algorithm);
    ctx.sharpness = m_options.scaler.sharpness;
    ctx.baseFps = sn.baseFps;
    ctx.frameGenEnabled = false;
    for (auto it = s.fpsByHeight.rbegin(); it != s.fpsByHeight.rend(); ++it) {
        if (it->first > sn.srcH && it->second.count > 0) {
            ctx.higherResBaseFps = it->second.sum / it->second.count;
            ctx.higherResSrcHeight = it->first;
            break;
        }
    }
    sn.tips = ff::recommend(ctx);

    for (const ff::Tip& tip : sn.tips) {
        if (!s.shownTips.insert(tip.id).second) continue;
        Log::write("Tip [%s]: %s", tip.id.c_str(), tip.text.c_str());
        if (tip.severity != ff::TipSeverity::Info) {
            s.toastTipText = tip.text + str::kTipHint;
            s.toastTipUntil = now + static_cast<long long>(6.0 / ticksToSeconds(1));
            s.renderPending = true;
        }
    }
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
        updateTips(sn);
        s.windowStart = nowTicks();
        s.baseFrames = s.outFrames = 0;
        s.cpuSeconds = 0;
    }
}

void App::focusTick() {
    Session& s = *m_session;
    const HWND fg = GetForegroundWindow();
    if (!fg || fg == s.overlay->hwnd()) return; // transitional states
    const bool focused = fg == s.target || GetAncestor(fg, GA_ROOTOWNER) == s.target;
    if (!s.paused && !focused) {
        s.paused = true;
        s.renderPending = false;
        ShowWindow(s.overlay->hwnd(), SW_HIDE);
        if (s.cursor) s.cursor->update(); // releases clip + restores the system cursor
        wchar_t ft[256] = L"";
        GetWindowTextW(fg, ft, 256);
        Log::write("Paused (target lost focus: %s)", toUtf8(ft).c_str());
    } else if (s.paused && focused) {
        // Render first (hidden), then show, so the stale back buffer is never visible.
        try {
            renderFrame();
        } catch (const std::exception& e) {
            Log::write("Render error: %s", e.what());
            stopScaling("error");
            return;
        }
        if (!m_session) return;
        s.paused = false;
        SetWindowPos(s.overlay->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(s.overlay->hwnd(), SW_SHOWNOACTIVATE);
        if (s.cursor) s.cursor->update();
        s.renderPending = true;
        Log::write("Resumed");
    }
}

bool App::cursorTick() {
    Session& s = *m_session;
    if (!s.cursor || !s.cursor->drawVirtualCursor()) return false;
    CURSORINFO ci{};
    ci.cbSize = sizeof ci;
    if (!GetCursorInfo(&ci)) return false;
    const bool showing = (ci.flags & CURSOR_SHOWING) != 0;
    const bool changed = showing != s.lastCursorShowing ||
                         (showing && (ci.ptScreenPos.x != s.lastCursorPos.x || ci.ptScreenPos.y != s.lastCursorPos.y ||
                                      ci.hCursor != s.lastCursor));
    s.lastCursorShowing = showing;
    s.lastCursorPos = ci.ptScreenPos;
    s.lastCursor = ci.hCursor;
    return changed;
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
        else {
            Session& ss = *m_session;
            const HWND fg = GetForegroundWindow();
            if (fg != ss.lastForeground) {
                wchar_t ft[256] = L"";
                if (fg) GetWindowTextW(fg, ft, 256);
                Log::write("foreground changed: hwnd=%p \"%s\"%s", static_cast<void*>(fg), toUtf8(ft).c_str(),
                           fg == ss.overlay->hwnd() ? " (OVERLAY!)" : "");
                ss.lastForeground = fg;
            }
            if (!ss.loggedFg1s && ticksToSeconds(now - ss.startTicks) >= 1.0) {
                ss.loggedFg1s = true;
                wchar_t ft[256] = L"";
                if (fg) GetWindowTextW(fg, ft, 256);
                Log::write("foreground +1s: %s (target is foreground: %s)", toUtf8(ft).c_str(),
                           fg == ss.target ? "yes" : "no");
            }
            if (m_session->cursor) m_session->cursor->update();
            flushStats();
        }
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
    Log::write("FreeFrame started (Ctrl+Alt+S: toggle scaling, Ctrl+Alt+O: statistics, Ctrl+Alt+A: algorithm, Ctrl+Alt+Up/Down: sharpness, Ctrl+Alt+Q: quit)");
    m_titlePending = !m_options.targetTitle.empty();
    if (m_titlePending && m_options.exitAfterSeconds <= 0)
        Log::write("Waiting for a window titled \"%s\"", toUtf8(m_options.targetTitle).c_str());

    bool quit = false;
    while (!quit) {
        // Wait for a new captured frame first, then for the swapchain waitable before rendering.
        HANDLE wait = nullptr;
        if (m_session) {
            const bool waitSwapchain = m_options.presentEveryVblank || m_session->renderPending;
            if (m_session->paused) wait = nullptr;
            else wait = waitSwapchain ? m_session->swapchain->waitable() : m_session->capture->frameEvent();
        }
        // Poll the cursor at ~250 Hz while the virtual cursor is active.
        const bool pollCursor = m_session && !m_session->paused && m_session->cursor && m_session->cursor->drawVirtualCursor();
        const DWORD r = MsgWaitForMultipleObjectsEx(wait ? 1u : 0u, wait ? &wait : nullptr, pollCursor ? 4u : (m_session && m_session->paused ? 30u : 100u),
                                                    QS_ALLINPUT, MWMO_INPUTAVAILABLE);

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

        if (wait && r == WAIT_OBJECT_0 && m_session) {
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
        if (m_session) focusTick();
        if (m_session && !m_session->paused && !m_session->renderPending && cursorTick()) m_session->renderPending = true;
        // Keep the statistics text alive (~10 Hz) even if the source is static.
        if (m_session && !m_session->paused && (m_showStats || m_session->toastShown ||
                                                 nowTicks() < std::max(m_session->toastSettingsUntil, m_session->toastTipUntil)) &&
            !m_session->renderPending &&
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
