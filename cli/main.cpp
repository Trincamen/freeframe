#include <cstdio>
#include <exception>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "freeframe/format.hpp"
#include "freeframe/gpu_backend.hpp"
#include "freeframe/version.hpp"

namespace {

constexpr int kExitUsage = 64;

void printUsage(std::FILE* out) {
    std::fprintf(out,
                 "Usage: freeframe-cli <command>\n"
                 "\n"
                 "Commands:\n"
                 "  gpus         List GPUs (Vulkan, and DXGI on Windows)\n"
                 "  --version    Print version\n"
                 "  --help       Show this help\n");
}

// Returns the number of GPUs listed.
size_t printSection(const char* title, ff::IGpuBackend& backend) {
    std::printf("%s:\n", title);
    std::fflush(stdout);
    const auto res = backend.enumerateAdapters();
    if (!res.ok()) {
        std::fprintf(stderr, "  error: %s\n", res.error().c_str());
        return 0;
    }
    const auto& gpus = res.value();
    for (size_t i = 0; i < gpus.size(); ++i) {
        const ff::GpuInfo& g = gpus[i];
        std::string line = ff::vendorName(g.vendorId);
        if (g.api == ff::GpuApi::Vulkan) {
            line += std::string(", ") + ff::to_string(g.type);
        }
        line += ", " + ff::formatBytes(g.dedicatedVideoMemoryBytes);
        if (!g.driverVersion.empty()) line += ", driver " + g.driverVersion;
        if (g.api == ff::GpuApi::Vulkan && !g.apiVersion.empty()) line += ", Vulkan " + g.apiVersion;
        std::printf("  [%zu] %s (%s)\n", i, g.name.c_str(), line.c_str());
    }
    return gpus.size();
}

int cmdGpus() {
    size_t total = 0;
    {
        auto vk = ff::createVulkanBackend();
        total += printSection("Vulkan", *vk);
    }
#ifdef _WIN32
    {
        auto dx = ff::createDxgiBackend();
        total += printSection("DXGI", *dx);
    }
#endif
    return total > 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 2) {
        printUsage(stdout);
        return 0;
    }
    const std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        printUsage(stdout);
        return 0;
    }
    if (cmd == "--version" || cmd == "-V") {
        std::printf("freeframe-cli %s\n", FREEFRAME_VERSION_STRING);
        return 0;
    }
    if (cmd == "gpus") {
        try {
            return cmdGpus();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "error: %s\n", e.what());
            return 1;
        }
    }
    std::fprintf(stderr, "freeframe-cli: unknown command \"%s\"\n\n", cmd.c_str());
    printUsage(stderr);
    return kExitUsage;
}
