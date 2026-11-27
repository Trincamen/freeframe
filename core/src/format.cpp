#include "freeframe/format.hpp"

#include <cstdio>

namespace ff {

std::string decodeDriverVersion(uint32_t vendorId, uint32_t v) {
    char buf[64];
    if (vendorId == 0x10DE) {
        std::snprintf(buf, sizeof buf, "%u.%02u", (v >> 22) & 0x3FFu, (v >> 14) & 0xFFu);
    } else {
        std::snprintf(buf, sizeof buf, "%u.%u.%u", (v >> 22) & 0x7Fu, (v >> 12) & 0x3FFu, v & 0xFFFu);
    }
    return buf;
}

std::string decodeVulkanApiVersion(uint32_t v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%u.%u.%u", (v >> 22) & 0x7Fu, (v >> 12) & 0x3FFu, v & 0xFFFu);
    return buf;
}

std::string formatBytes(uint64_t bytes) {
    constexpr uint64_t kMiB = 1024ull * 1024ull;
    constexpr uint64_t kGiB = 1024ull * kMiB;
    char buf[64];
    if (bytes >= kGiB) {
        std::snprintf(buf, sizeof buf, "%.1f GiB", static_cast<double>(bytes) / static_cast<double>(kGiB));
    } else if (bytes >= kMiB) {
        std::snprintf(buf, sizeof buf, "%llu MiB", static_cast<unsigned long long>(bytes / kMiB));
    } else {
        std::snprintf(buf, sizeof buf, "%llu B", static_cast<unsigned long long>(bytes));
    }
    return buf;
}

} // namespace ff
