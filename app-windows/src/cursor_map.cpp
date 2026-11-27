#include "cursor_map.hpp"

#include <magnification.h>

#include <atomic>
#include <cstdlib>
#include <vector>

#include "log.hpp"

namespace ffapp {

namespace {

std::atomic<bool> g_clipped{false};
std::atomic<bool> g_hidden{false};
std::atomic<bool> g_magInit{false};

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS*) {
    restoreCursorStateGlobal();
    return EXCEPTION_CONTINUE_SEARCH;
}

bool isTargetForeground(HWND target) {
    const HWND fg = GetForegroundWindow();
    return fg && (fg == target || GetAncestor(fg, GA_ROOTOWNER) == target || GetAncestor(target, GA_ROOT) == fg);
}

bool targetClientRect(HWND target, RECT& out) {
    RECT cr{};
    POINT o{0, 0};
    if (!GetClientRect(target, &cr) || !ClientToScreen(target, &o)) return false;
    out = {o.x, o.y, o.x + cr.right, o.y + cr.bottom};
    return out.right > out.left && out.bottom > out.top;
}

} // namespace

void restoreCursorStateGlobal() {
    if (g_clipped.exchange(false)) ClipCursor(nullptr);
    if (g_hidden.exchange(false)) MagShowSystemCursor(TRUE);
    if (g_magInit.exchange(false)) MagUninitialize();
}

void installCursorSafetyNet() {
    static bool done = false;
    if (done) return;
    done = true;
    std::atexit([] { restoreCursorStateGlobal(); });
    SetUnhandledExceptionFilter(&unhandledFilter);
}

// ---------------------------------------------------------------- CursorMapper

void CursorMapper::start(HWND target) {
    stop();
    installCursorSafetyNet();
    m_target = target;
    m_started = true;
    if (MagInitialize()) {
        m_magOk = true;
        g_magInit = true;
    } else {
        Log::write("MagInitialize failed (error %lu); real cursor stays visible, virtual cursor disabled",
                   GetLastError());
    }
    update();
}

void CursorMapper::stop() {
    if (!m_started) return;
    release();
    if (m_magOk) {
        if (g_magInit.exchange(false)) MagUninitialize();
        m_magOk = false;
    }
    m_started = false;
}

bool CursorMapper::clientOrigin(POINT& p) const {
    RECT r{};
    if (!m_started || !targetClientRect(m_target, r)) return false;
    p = {r.left, r.top};
    return true;
}

void CursorMapper::engage(const RECT& r) {
    if (!m_clipped || !EqualRect(&r, &m_rect)) {
        if (ClipCursor(&r)) {
            if (!m_clipped)
                Log::write("Cursor clip applied (%ld,%ld)-(%ld,%ld)", r.left, r.top, r.right, r.bottom);
            m_clipped = true;
            g_clipped = true;
            m_rect = r;
        } else {
            Log::write("ClipCursor failed (error %lu)", GetLastError());
        }
    }
    if (!m_hidden && m_magOk && m_clipped) {
        if (MagShowSystemCursor(FALSE)) {
            m_hidden = true;
            g_hidden = true;
            Log::write("System cursor hidden (virtual cursor drawn on overlay)");
        } else {
            m_magOk = false; // do not retry every tick
            Log::write("MagShowSystemCursor(FALSE) failed (error %lu); real cursor stays visible", GetLastError());
        }
    }
}

void CursorMapper::release() {
    if (m_clipped) {
        ClipCursor(nullptr);
        m_clipped = false;
        g_clipped = false;
        Log::write("Cursor clip released");
    }
    if (m_hidden) {
        MagShowSystemCursor(TRUE);
        m_hidden = false;
        g_hidden = false;
        Log::write("System cursor restored");
    }
}

void CursorMapper::update() {
    if (!m_started) return;
    RECT r{};
    if (IsWindow(m_target) && !IsIconic(m_target) && isTargetForeground(m_target) && targetClientRect(m_target, r))
        engage(r);
    else
        release();
}

// ---------------------------------------------------------------- CursorCache

const CursorSprite* CursorCache::get(HCURSOR cursor) {
    if (!cursor) return nullptr;
    if (m_cache.size() > 64) m_cache.clear(); // animated cursors create many handles
    auto it = m_cache.find(cursor);
    if (it != m_cache.end()) return it->second.valid ? &it->second.sprite : nullptr;

    Entry& e = m_cache[cursor];
    ICONINFO ii{};
    if (!GetIconInfo(cursor, &ii)) return nullptr;
    struct Gdi {
        ICONINFO& i;
        ~Gdi() {
            if (i.hbmColor) DeleteObject(i.hbmColor);
            if (i.hbmMask) DeleteObject(i.hbmMask);
        }
    } gdi{ii};

    BITMAP bm{};
    const bool mono = ii.hbmColor == nullptr;
    if (!GetObjectW(mono ? ii.hbmMask : ii.hbmColor, sizeof bm, &bm)) return nullptr;
    const int w = bm.bmWidth;
    const int h = mono ? bm.bmHeight / 2 : bm.bmHeight;
    if (w <= 0 || h <= 0 || w > 512 || h > 512) return nullptr;

    HDC dc = GetDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = mono ? -(h * 2) : -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> mask(static_cast<size_t>(w) * h * (mono ? 2 : 1), 0);
    std::vector<uint32_t> color;
    bool ok = GetDIBits(dc, ii.hbmMask, 0, static_cast<UINT>(h * (mono ? 2 : 1)), mask.data(), &bi, DIB_RGB_COLORS) != 0;
    if (ok && !mono) {
        bi.bmiHeader.biHeight = -h;
        color.assign(static_cast<size_t>(w) * h, 0);
        ok = GetDIBits(dc, ii.hbmColor, 0, static_cast<UINT>(h), color.data(), &bi, DIB_RGB_COLORS) != 0;
    }
    ReleaseDC(nullptr, dc);
    if (!ok) return nullptr;

    // Build straight-alpha RGBA (R in the low byte).
    std::vector<uint32_t> rgba(static_cast<size_t>(w) * h, 0);
    if (mono) {
        for (int i = 0; i < w * h; ++i) {
            const bool a = (mask[static_cast<size_t>(i)] & 0xFFFFFF) != 0;                         // AND
            const bool x = (mask[static_cast<size_t>(i) + static_cast<size_t>(w) * h] & 0xFFFFFF) != 0; // XOR
            if (!a && !x) rgba[static_cast<size_t>(i)] = 0xFF000000u;       // black
            else if (!a && x) rgba[static_cast<size_t>(i)] = 0xFFFFFFFFu;   // white
            else if (a && x) rgba[static_cast<size_t>(i)] = 0xFFFFFFFFu;    // invert: approximated as white
            // a && !x: transparent
        }
    } else {
        bool anyAlpha = false;
        for (uint32_t px : color) anyAlpha |= (px >> 24) != 0;
        for (int i = 0; i < w * h; ++i) {
            const uint32_t bgra = color[static_cast<size_t>(i)];
            const uint32_t b = bgra & 0xFF, g = (bgra >> 8) & 0xFF, r = (bgra >> 16) & 0xFF;
            uint32_t a = bgra >> 24;
            if (!anyAlpha) a = (mask[static_cast<size_t>(i)] & 0xFFFFFF) ? 0 : 255; // legacy AND mask
            rgba[static_cast<size_t>(i)] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{rgba.data(), static_cast<UINT>(w) * 4u, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (FAILED(m_device->CreateTexture2D(&td, &sd, &tex))) return nullptr;
    if (FAILED(m_device->CreateShaderResourceView(tex.Get(), nullptr, &e.srvOwner))) return nullptr;

    e.sprite = {e.srvOwner.Get(), static_cast<UINT>(w), static_cast<UINT>(h), static_cast<int>(ii.xHotspot),
                static_cast<int>(ii.yHotspot)};
    e.valid = true;
    size_t opaque = 0;
    for (uint32_t px : rgba) opaque += (px >> 24) != 0;
    Log::write("Cursor sprite %dx%d hotspot (%lu,%lu) %s, %zu visible pixels", w, h, ii.xHotspot, ii.yHotspot,
               mono ? "monochrome" : "color", opaque);
    return &e.sprite;
}

} // namespace ffapp
