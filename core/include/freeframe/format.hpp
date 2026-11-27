#pragma once

#include <cstdint>
#include <string>

namespace ff {

// Decodes VkPhysicalDeviceProperties::driverVersion. NVIDIA (0x10DE) uses a custom
// layout "major.minor" (10/8 bits); everyone else uses the Vulkan API version layout.
std::string decodeDriverVersion(uint32_t vendorId, uint32_t driverVersion);

// Decodes a packed Vulkan API version (variant ignored) as "major.minor.patch".
std::string decodeVulkanApiVersion(uint32_t apiVersion);

// "512 MiB", "12.0 GiB", "0 B".
std::string formatBytes(uint64_t bytes);

} // namespace ff
