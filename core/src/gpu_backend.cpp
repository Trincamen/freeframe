#include "freeframe/gpu_backend.hpp"

namespace ff {

const char* to_string(GpuApi api) noexcept {
    switch (api) {
    case GpuApi::Vulkan: return "Vulkan";
    case GpuApi::D3D11: return "D3D11";
    }
    return "Unknown";
}

const char* to_string(GpuType type) noexcept {
    switch (type) {
    case GpuType::Discrete: return "discrete";
    case GpuType::Integrated: return "integrated";
    case GpuType::Virtual: return "virtual";
    case GpuType::Cpu: return "cpu";
    case GpuType::Other: return "other";
    }
    return "other";
}

const char* vendorName(uint32_t vendorId) noexcept {
    switch (vendorId) {
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "AMD";
    case 0x8086: return "Intel";
    case 0x1414: return "Microsoft";
    case 0x5143: return "Qualcomm";
    default: return "Unknown";
    }
}

} // namespace ff
