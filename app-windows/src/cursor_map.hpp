#pragma once
// Cursor mapping for the overlay: the real cursor is clipped to the target window's client area (so the game
// keeps receiving mouse input at its real position), hidden system-wide through the Magnification API, and a
// scaled copy is drawn by the renderer on the overlay.
#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <unordered_map>

namespace ffapp {

struct CursorSprite {
    ID3D11ShaderResourceView* srv = nullptr;
    UINT width = 0, height = 0;
    int hotX = 0, hotY = 0;
};

// HCURSOR -> RGBA texture (straight alpha) cache.
class CursorCache {
public:
    explicit CursorCache(ID3D11Device* device) : m_device(device) {}
    // Returns null if the cursor cannot be converted. Pointer valid until the next get().
    const CursorSprite* get(HCURSOR cursor);

private:
    struct Entry {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvOwner;
        CursorSprite sprite;
        bool valid = false;
    };
    ID3D11Device* m_device;
    std::unordered_map<HCURSOR, Entry> m_cache;
};

class CursorMapper {
public:
    CursorMapper() = default;
    ~CursorMapper() { stop(); }
    CursorMapper(const CursorMapper&) = delete;
    CursorMapper& operator=(const CursorMapper&) = delete;

    void start(HWND target);
    void stop(); // always restores clip and system cursor
    // Re-evaluates focus/window rectangle; applies or releases clip and cursor hiding.
    void update();
    // True when the real cursor is hidden and the renderer must draw the virtual one.
    bool drawVirtualCursor() const noexcept { return m_hidden; }
    // Client-area origin of the target in screen coordinates.
    bool clientOrigin(POINT& p) const;

private:
    void engage(const RECT& r);
    void release();
    HWND m_target = nullptr;
    bool m_started = false;
    bool m_magOk = false;
    bool m_clipped = false;
    bool m_hidden = false;
    RECT m_rect{};
};

// Idempotent last-resort restore (clip + system cursor); used by atexit / unhandled-exception handlers.
void restoreCursorStateGlobal();
void installCursorSafetyNet();
// If a previous instance died with the cursor clipped/hidden (lock file present), repairs it. Returns true if so.
bool recoverCursorStateIfNeeded();

} // namespace ffapp
