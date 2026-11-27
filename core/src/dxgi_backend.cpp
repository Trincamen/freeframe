#include <windows.h>
#include <dxgi.h>

#include <cstdio>
#include <string>
#include <vector>

#include "freeframe/gpu_backend.hpp"

namespace ff {

namespace {

std::string wideToUtf8(const wchar_t* w) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
    out.resize(static_cast<size_t>(len - 1));
    return out;
}

template <typename T>
struct ComRelease {
    T* p = nullptr;
    ComRelease() = default;
    ComRelease(const ComRelease&) = delete;
    ComRelease& operator=(const ComRelease&) = delete;
    ~ComRelease() {
        if (p) p->Release();
    }
};

std::string toHex(unsigned long v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08lX", v);
    return buf;
}

class DxgiBackend final : public IGpuBackend {
public:
    GpuApi api() const override { return GpuApi::D3D11; }

    Result<std::vector<GpuInfo>> enumerateAdapters() override {
        using R = Result<std::vector<GpuInfo>>;

        ComRelease<IDXGIFactory1> factory;
        const HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory.p));
        if (FAILED(hr) || !factory.p) {
            return R::failure("CreateDXGIFactory1 failed (HRESULT 0x" + toHex(static_cast<unsigned long>(hr)) + ")");
        }

        std::vector<GpuInfo> result;
        std::vector<LUID> seenLuids;
        for (UINT i = 0;; ++i) {
            ComRelease<IDXGIAdapter1> adapter;
            if (factory.p->EnumAdapters1(i, &adapter.p) == DXGI_ERROR_NOT_FOUND) break;
            if (!adapter.p) break;

            DXGI_ADAPTER_DESC1 desc{};
            if (FAILED(adapter.p->GetDesc1(&desc))) continue;

            // EnumAdapters1 can report the same adapter more than once; skip LUIDs already seen.
            bool duplicate = false;
            for (const LUID& seen : seenLuids) {
                if (seen.LowPart == desc.AdapterLuid.LowPart && seen.HighPart == desc.AdapterLuid.HighPart) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;
            seenLuids.push_back(desc.AdapterLuid);

            GpuInfo info;
            info.name = wideToUtf8(desc.Description);
            info.api = GpuApi::D3D11;
            info.type = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? GpuType::Cpu : GpuType::Other;
            info.vendorId = desc.VendorId;
            info.deviceId = desc.DeviceId;
            info.dedicatedVideoMemoryBytes = static_cast<uint64_t>(desc.DedicatedVideoMemory);
            result.push_back(std::move(info));
        }

        if (result.empty()) return R::failure("no DXGI adapters found");
        return R::success(std::move(result));
    }
};

} // namespace

std::unique_ptr<IGpuBackend> createDxgiBackend() {
    return std::make_unique<DxgiBackend>();
}

} // namespace ff
