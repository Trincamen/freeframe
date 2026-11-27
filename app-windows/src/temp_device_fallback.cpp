// TEMP(M1-S2): remover após integração do S1 (core d3d11 backend).
#include "temp_device_fallback.hpp"

#include <algorithm>

namespace ffapp {

using ff::d3d11::Device;
using ff::d3d11::Rect;

ff::Result<Device> tempCreateDeviceFallback() {
    Device d;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   levels, 2, D3D11_SDK_VERSION, d.device.GetAddressOf(), nullptr,
                                   d.context.GetAddressOf());
    if (hr == E_INVALIDARG) { // runtime without 11_1
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               levels + 1, 1, D3D11_SDK_VERSION, d.device.ReleaseAndGetAddressOf(), nullptr,
                               d.context.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr)) return ff::Result<Device>::failure("D3D11CreateDevice failed");
    ff::d3d11::ComPtr<IDXGIDevice> dxgi;
    if (SUCCEEDED(d.device.As(&dxgi))) {
        ff::d3d11::ComPtr<IDXGIAdapter> a;
        if (SUCCEEDED(dxgi->GetAdapter(&a))) {
            a.As(&d.adapter);
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(a->GetDesc(&desc))) d.luid = desc.AdapterLuid;
        }
    }
    return ff::Result<Device>::success(std::move(d));
}

void tempCopyCentered(ID3D11DeviceContext* ctx, ID3D11Texture2D* src, Rect srcRect, ID3D11Texture2D* dst) {
    D3D11_TEXTURE2D_DESC dd{};
    dst->GetDesc(&dd);
    const uint32_t w = std::min(srcRect.width, dd.Width);
    const uint32_t h = std::min(srcRect.height, dd.Height);
    if (w == 0 || h == 0) return;
    const uint32_t dx = (dd.Width - w) / 2;
    const uint32_t dy = (dd.Height - h) / 2;
    const uint32_t sx = srcRect.x + (srcRect.width - w) / 2;
    const uint32_t sy = srcRect.y + (srcRect.height - h) / 2;
    const D3D11_BOX box{sx, sy, 0, sx + w, sy + h, 1};
    ctx->CopySubresourceRegion(dst, 0, dx, dy, 0, src, 0, &box);
}

} // namespace ffapp
