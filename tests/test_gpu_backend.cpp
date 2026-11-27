#include <doctest/doctest.h>

#include <string>

#include "freeframe/gpu_backend.hpp"

TEST_CASE("vendorName") {
    CHECK(std::string(ff::vendorName(0x10DE)) == "NVIDIA");
    CHECK(std::string(ff::vendorName(0x1002)) == "AMD");
    CHECK(std::string(ff::vendorName(0x8086)) == "Intel");
    CHECK(std::string(ff::vendorName(0x1414)) == "Microsoft");
    CHECK(std::string(ff::vendorName(0x5143)) == "Qualcomm");
    CHECK(std::string(ff::vendorName(0x1234)) == "Unknown");
}

TEST_CASE("enum toString") {
    CHECK(std::string(ff::to_string(ff::GpuApi::Vulkan)) == "Vulkan");
    CHECK(std::string(ff::to_string(ff::GpuApi::D3D11)) == "D3D11");
    CHECK(std::string(ff::to_string(ff::GpuType::Discrete)) == "discrete");
    CHECK(std::string(ff::to_string(ff::GpuType::Integrated)) == "integrated");
    CHECK(std::string(ff::to_string(ff::GpuType::Virtual)) == "virtual");
    CHECK(std::string(ff::to_string(ff::GpuType::Cpu)) == "cpu");
    CHECK(std::string(ff::to_string(ff::GpuType::Other)) == "other");
}

TEST_CASE("Vulkan enumeration does not crash") {
    auto backend = ff::createVulkanBackend();
    REQUIRE(backend != nullptr);
    CHECK(backend->api() == ff::GpuApi::Vulkan);
    const auto res = backend->enumerateAdapters();
    if (res.ok()) {
        CHECK_FALSE(res.value().empty());
    } else {
        CHECK_FALSE(res.error().empty());
    }
}
