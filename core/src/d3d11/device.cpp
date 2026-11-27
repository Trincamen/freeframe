#include <dxgi1_6.h>

#include <exception>

#include "util.hpp"

namespace ff::d3d11 {

using detail::fail;

namespace {

HRESULT createOnAdapter(IDXGIAdapter1* adapter, UINT flags, Device& out) {
    static const D3D_FEATURE_LEVEL kLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, kLevels, 2, D3D11_SDK_VERSION,
                                   out.device.ReleaseAndGetAddressOf(), &got, out.context.ReleaseAndGetAddressOf());
    if (hr == E_INVALIDARG) { // runtime without feature level 11_1 rejects the whole list
        hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, kLevels + 1, 1, D3D11_SDK_VERSION,
                               out.device.ReleaseAndGetAddressOf(), &got, out.context.ReleaseAndGetAddressOf());
    }
    return hr;
}

bool isHardware(IDXGIAdapter1* a) {
    DXGI_ADAPTER_DESC1 d{};
    return SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
}

} // namespace

Result<Device> createDevice(std::optional<LUID> luid, bool debugLayer) {
    try {
        ComPtr<IDXGIFactory2> factory;
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf()));
        if (FAILED(hr)) return fail<Device>("CreateDXGIFactory2", hr);

        Device dev;
        if (luid) {
            ComPtr<IDXGIFactory4> f4;
            hr = factory.As(&f4);
            if (FAILED(hr)) return fail<Device>("IDXGIFactory4 (EnumAdapterByLuid)", hr);
            hr = f4->EnumAdapterByLuid(*luid, IID_PPV_ARGS(dev.adapter.GetAddressOf()));
            if (FAILED(hr)) return fail<Device>("EnumAdapterByLuid", hr);
        } else {
            ComPtr<IDXGIFactory6> f6;
            ComPtr<IDXGIFactory1> f1;
            if (SUCCEEDED(factory.As(&f6))) {
                for (UINT i = 0;; ++i) {
                    ComPtr<IDXGIAdapter1> a;
                    if (f6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                       IID_PPV_ARGS(a.GetAddressOf())) == DXGI_ERROR_NOT_FOUND)
                        break;
                    if (isHardware(a.Get())) {
                        dev.adapter = a;
                        break;
                    }
                }
            } else if (SUCCEEDED(factory.As(&f1))) {
                for (UINT i = 0;; ++i) {
                    ComPtr<IDXGIAdapter1> a;
                    if (f1->EnumAdapters1(i, a.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) break;
                    if (isHardware(a.Get())) {
                        dev.adapter = a;
                        break;
                    }
                }
            }
            if (!dev.adapter) return Result<Device>::failure("no hardware DXGI adapter found");
        }

        DXGI_ADAPTER_DESC1 desc{};
        hr = dev.adapter->GetDesc1(&desc);
        if (FAILED(hr)) return fail<Device>("IDXGIAdapter1::GetDesc1", hr);
        dev.luid = desc.AdapterLuid;

        const UINT baseFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        hr = E_FAIL;
        if (debugLayer) hr = createOnAdapter(dev.adapter.Get(), baseFlags | D3D11_CREATE_DEVICE_DEBUG, dev);
        if (!debugLayer || FAILED(hr)) { // debug layer not installed: fall back silently
            hr = createOnAdapter(dev.adapter.Get(), baseFlags, dev);
        }
        if (FAILED(hr)) return fail<Device>("D3D11CreateDevice", hr);
        return Result<Device>::success(std::move(dev));
    } catch (const std::exception& e) {
        return Result<Device>::failure(std::string("createDevice: ") + e.what());
    } catch (...) {
        return Result<Device>::failure("createDevice: unknown exception");
    }
}

} // namespace ff::d3d11
