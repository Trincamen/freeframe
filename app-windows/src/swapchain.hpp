#pragma once
#include <d3d11.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <string>

namespace ffapp {

// Flip-model (discard) swapchain with a frame-latency waitable object, bound to the overlay window.
class Swapchain {
public:
    struct Config {
        uint32_t width = 0, height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bool unorderedAccess = true; // BufferUsage |= UNORDERED_ACCESS (required by the real Scaler)
        bool allowTearing = true;    // used only if the system supports it
    };

    static std::unique_ptr<Swapchain> create(ID3D11Device* device, HWND hwnd, const Config& cfg, std::string& error);

    HANDLE waitable() const noexcept { return m_waitable; }
    ID3D11Texture2D* backBuffer() const noexcept { return m_backBuffer.Get(); }
    ID3D11RenderTargetView* rtv() const noexcept { return m_rtv.Get(); }
    ID3D11UnorderedAccessView* uav() const noexcept { return m_uav.Get(); } // null if not requested
    uint32_t width() const noexcept { return m_cfg.width; }
    uint32_t height() const noexcept { return m_cfg.height; }
    bool tearingAvailable() const noexcept { return m_tearing; }

    // vsync=false presents immediately (with tearing if available).
    HRESULT present(bool vsync);

    ~Swapchain();
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

private:
    Swapchain() = default;
    Config m_cfg;
    bool m_tearing = false;
    HANDLE m_waitable = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> m_swapchain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backBuffer;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_uav;
};

} // namespace ffapp
