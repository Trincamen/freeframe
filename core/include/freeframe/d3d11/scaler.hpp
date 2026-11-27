#pragma once
// D3D11 compute backend for the overlay mode (Windows only). Contract between core (implementation)
// and app-windows (consumer). Changing this header requires the orchestrator's approval.

#ifndef _WIN32
#error "freeframe/d3d11/scaler.hpp is Windows-only"
#endif

#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include "freeframe/result.hpp"

namespace ff::d3d11 {

using Microsoft::WRL::ComPtr;
using Status = Result<std::monostate>;
inline Status okStatus() { return Status::success({}); }

struct Device {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIAdapter1> adapter;
    LUID luid{};
};

// Creates a hardware D3D11 device (feature level 11_0+, BGRA support) on the adapter with `luid`,
// or, if not given, on the high-performance adapter (IDXGIFactory6::EnumAdapterByGpuPreference).
// Pass debugLayer=true to enable D3D11_CREATE_DEVICE_DEBUG (falls back silently if unavailable).
Result<Device> createDevice(std::optional<LUID> luid = std::nullopt, bool debugLayer = false);

enum class ScaleAlgorithm { Nearest, Bilinear, Fsr1 };

struct ScalerSettings {
    ScaleAlgorithm algorithm = ScaleAlgorithm::Fsr1;
    float sharpness = 0.5f; // 0 = none, 1 = max. Used by FSR1 (RCAS); ignored by Nearest/Bilinear.
};

// Rectangle in pixels.
struct Rect {
    uint32_t x = 0, y = 0, width = 0, height = 0;
};

// Upscales `src` (SRV, typically DXGI_FORMAT_B8G8R8A8_UNORM from capture) into `dst`
// (UAV of a DXGI_FORMAT_R8G8B8A8_UNORM texture, e.g. a swapchain back buffer created with
// DXGI_USAGE_UNORDERED_ACCESS). Owns its shaders, constant buffers and intermediate textures.
// Not thread-safe; use from the thread that owns the immediate context.
class Scaler {
public:
    static Result<std::unique_ptr<Scaler>> create(ID3D11Device* device);
    virtual ~Scaler() = default;

    // srcRect: region of the source texture to scale (e.g. client area of the captured window).
    // dstRect: region of the destination to write (e.g. aspect-fit inside the monitor).
    // Pixels of dst outside dstRect are not touched (the caller clears them).
    virtual Status run(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src, Rect srcRect,
                       ID3D11UnorderedAccessView* dst, Rect dstRect, const ScalerSettings& settings) = 0;
};

// Per-stage GPU timing via timestamp queries (D3D11_QUERY_TIMESTAMP + TIMESTAMP_DISJOINT).
// Results are read back non-blockingly a few frames later.
enum class Stage : uint32_t { Capture = 0, Scale = 1, Overlay = 2, Count = 3 };

class GpuTimer {
public:
    static Result<std::unique_ptr<GpuTimer>> create(ID3D11Device* device);
    virtual ~GpuTimer() = default;

    virtual void beginFrame(ID3D11DeviceContext* ctx) = 0;
    virtual void beginStage(ID3D11DeviceContext* ctx, Stage stage) = 0;
    virtual void endStage(ID3D11DeviceContext* ctx, Stage stage) = 0;
    virtual void endFrame(ID3D11DeviceContext* ctx) = 0;

    // Latest completed measurements in milliseconds (0 for stages not measured). Never blocks.
    virtual std::array<double, static_cast<size_t>(Stage::Count)> lastStageMs() const = 0;
};

} // namespace ff::d3d11
