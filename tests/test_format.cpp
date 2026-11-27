#include <doctest/doctest.h>

#include "freeframe/format.hpp"

TEST_CASE("decodeDriverVersion NVIDIA") {
    // 566.36 -> major 566 (10 bits at 22), minor 36 (8 bits at 14)
    const uint32_t v = (566u << 22) | (36u << 14);
    CHECK(ff::decodeDriverVersion(0x10DE, v) == "566.36");
}

TEST_CASE("decodeDriverVersion generic") {
    const uint32_t v = (2u << 22) | (3u << 12) | 4u;
    CHECK(ff::decodeDriverVersion(0x1002, v) == "2.3.4");
    CHECK(ff::decodeDriverVersion(0x8086, v) == "2.3.4");
}

TEST_CASE("decodeVulkanApiVersion") {
    CHECK(ff::decodeVulkanApiVersion((1u << 22) | (3u << 12) | 289u) == "1.3.289");
}

TEST_CASE("formatBytes") {
    CHECK(ff::formatBytes(0) == "0 B");
    CHECK(ff::formatBytes(512) == "512 B");
    CHECK(ff::formatBytes(512ull * 1024 * 1024) == "512 MiB");
    CHECK(ff::formatBytes(12ull * 1024 * 1024 * 1024) == "12.0 GiB");
    CHECK(ff::formatBytes(1536ull * 1024 * 1024) == "1.5 GiB");
}
