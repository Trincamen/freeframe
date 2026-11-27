#include "swapchain.hpp"

#include <dxgi1_2.h>

namespace ffapp {

using Microsoft::WRL::ComPtr;

namespace {
std::string hrText(const char* what, HRESULT hr) {
    char buf[96];
    snprintf(buf, sizeof buf, "%s failed (0x%08lX)", what, static_cast<unsigned long>(hr));
    return buf;
}
} // namespace

std::unique_ptr<Swapchain> Swapchain::create(ID3D11Device* device, HWND hwnd, const Config& cfg,
                                             std::string& error) {
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
    if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        error = hrText("Get DXGI factory", hr);
        return nullptr;
    }

    std::unique_ptr<Swapchain> sc(new Swapchain());
    sc->m_cfg = cfg;

    ComPtr<IDXGIFactory5> factory5;
    if (cfg.allowTearing && SUCCEEDED(factory.As(&factory5))) {
        BOOL allow = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof allow)))
            sc->m_tearing = allow != FALSE;
    }

    DXGI_SWAP_CHAIN_DESC1 d{};
    d.Width = cfg.width;
    d.Height = cfg.height;
    d.Format = cfg.format;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | (cfg.unorderedAccess ? DXGI_USAGE_UNORDERED_ACCESS : 0u);
    d.BufferCount = 3;
    d.Scaling = DXGI_SCALING_NONE;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    d.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
              (sc->m_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u);

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(device, hwnd, &d, nullptr, nullptr, &sc1);
    if (FAILED(hr)) {
        error = hrText("CreateSwapChainForHwnd", hr);
        return nullptr;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    hr = sc1.As(&sc->m_swapchain);
    if (FAILED(hr)) {
        error = hrText("Query IDXGISwapChain2", hr);
        return nullptr;
    }
    sc->m_swapchain->SetMaximumFrameLatency(1);
    sc->m_waitable = sc->m_swapchain->GetFrameLatencyWaitableObject();

    hr = sc->m_swapchain->GetBuffer(0, IID_PPV_ARGS(&sc->m_backBuffer));
    if (SUCCEEDED(hr)) hr = device->CreateRenderTargetView(sc->m_backBuffer.Get(), nullptr, &sc->m_rtv);
    if (SUCCEEDED(hr) && cfg.unorderedAccess) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC u{};
        u.Format = cfg.format;
        u.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        hr = device->CreateUnorderedAccessView(sc->m_backBuffer.Get(), &u, &sc->m_uav);
    }
    if (FAILED(hr)) {
        error = hrText("Create back buffer views", hr);
        return nullptr;
    }
    return sc;
}

HRESULT Swapchain::present(bool vsync) {
    if (vsync) return m_swapchain->Present(1, 0);
    return m_swapchain->Present(0, m_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0u);
}

Swapchain::~Swapchain() {
    if (m_swapchain) {
        // Leave windowed/flip state clean before the window goes away.
        m_swapchain->SetFullscreenState(FALSE, nullptr);
    }
    // The waitable handle is owned by us (GetFrameLatencyWaitableObject returns a duplicate-like handle).
    if (m_waitable) CloseHandle(m_waitable);
}

} // namespace ffapp
