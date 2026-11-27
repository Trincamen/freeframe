#pragma once
#include <d3d11.h>
#include <windows.h>

#include <array>
#include <cstdint>

namespace ffapp {

struct StatsSnapshot {
    double baseFps = 0;
    double outFps = 0;
    double cpuMs = 0;
    bool gpuValid = false;
    std::array<double, 3> gpuMs{}; // Capture, Scale, Overlay
    const char* algorithm = "";
    // Source (captured client area) and output (aspect-fit) rectangles, in pixels. 0 = unknown.
    uint32_t srcW = 0, srcH = 0, dstW = 0, dstH = 0;
};

// Cursor sprite to draw on top of everything (screen pixels of the overlay).
struct CursorDraw {
    ID3D11ShaderResourceView* srv = nullptr;
    float x = 0, y = 0, w = 0, h = 0;
};

// Dear ImGui text overlay (top-left, translucent background). Owns the ImGui context.
class StatsOverlay {
public:
    StatsOverlay() = default;
    ~StatsOverlay();
    StatsOverlay(const StatsOverlay&) = delete;
    StatsOverlay& operator=(const StatsOverlay&) = delete;

    bool init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* ctx);
    void shutdown();
    // Draws into `rtv` (already containing the scaled image). Binds the RTV itself.
    void render(ID3D11RenderTargetView* rtv, UINT width, UINT height, const StatsSnapshot& stats,
                bool showStats, const CursorDraw* cursor);

private:
    bool m_initialized = false;
    ID3D11DeviceContext* m_ctx = nullptr;
    float m_scale = 1.0f;
};

} // namespace ffapp
