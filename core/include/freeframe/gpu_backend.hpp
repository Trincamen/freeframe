#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "freeframe/result.hpp"

namespace ff {

enum class GpuApi { Vulkan, D3D11 };
enum class GpuType { Discrete, Integrated, Virtual, Cpu, Other };

struct GpuInfo {
    std::string name;
    GpuApi api = GpuApi::Vulkan;
    GpuType type = GpuType::Other;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint64_t dedicatedVideoMemoryBytes = 0;
    std::string driverVersion;
    std::string apiVersion; // empty when not meaningful (DXGI)
};

const char* to_string(GpuApi api) noexcept;
const char* to_string(GpuType type) noexcept;
const char* vendorName(uint32_t vendorId) noexcept;

class IGpuBackend {
public:
    IGpuBackend() = default;
    IGpuBackend(const IGpuBackend&) = delete;
    IGpuBackend& operator=(const IGpuBackend&) = delete;
    virtual ~IGpuBackend() = default;

    virtual GpuApi api() const = 0;
    virtual Result<std::vector<GpuInfo>> enumerateAdapters() = 0;
};

std::unique_ptr<IGpuBackend> createVulkanBackend();

#ifdef _WIN32
// Enumerates adapters through DXGI; reports them as GpuApi::D3D11 (no device is created yet).
std::unique_ptr<IGpuBackend> createDxgiBackend();
#endif

} // namespace ff
