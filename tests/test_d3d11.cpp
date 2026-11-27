// D3D11 scaler tests (Windows only; skipped when no hardware adapter is available).
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

#include "freeframe/d3d11/scaler.hpp"

using namespace ff::d3d11;

namespace {

constexpr uint32_t kSentinel = 0xFF12AB34u;

struct Fixture {
    Device dev;
    std::unique_ptr<Scaler> scaler;
    bool ok = false;

    Fixture() {
        auto d = createDevice();
        if (!d.ok()) {
            MESSAGE("no hardware D3D11 device, skipping: " << d.error());
            return;
        }
        dev = d.value();
        auto s = Scaler::create(dev.device.Get());
        REQUIRE_MESSAGE(s.ok(), s.error());
        scaler = std::move(s).value();
        ok = true;
    }

    // Runs `algo` from `srcPixels` (BGRA, srcW x srcH) into a dstW x dstH RGBA texture pre-filled with kSentinel.
    // Returns the read back pixels as RGBA bytes packed in uint32 (R in the low byte).
    std::vector<uint32_t> run(const std::vector<uint32_t>& srcPixels, uint32_t srcW, uint32_t srcH, Rect srcRect,
                              uint32_t dstW, uint32_t dstH, Rect dstRect, ScalerSettings settings) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = srcW;
        td.Height = srcH;
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{srcPixels.data(), srcW * 4, 0};
        ComPtr<ID3D11Texture2D> srcTex;
        REQUIRE(SUCCEEDED(dev.device->CreateTexture2D(&td, &init, srcTex.GetAddressOf())));
        ComPtr<ID3D11ShaderResourceView> srv;
        REQUIRE(SUCCEEDED(dev.device->CreateShaderResourceView(srcTex.Get(), nullptr, srv.GetAddressOf())));

        std::vector<uint32_t> fill(static_cast<size_t>(dstW) * dstH, kSentinel);
        td.Width = dstW;
        td.Height = dstH;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        D3D11_SUBRESOURCE_DATA init2{fill.data(), dstW * 4, 0};
        ComPtr<ID3D11Texture2D> dstTex;
        REQUIRE(SUCCEEDED(dev.device->CreateTexture2D(&td, &init2, dstTex.GetAddressOf())));
        ComPtr<ID3D11UnorderedAccessView> uav;
        REQUIRE(SUCCEEDED(dev.device->CreateUnorderedAccessView(dstTex.Get(), nullptr, uav.GetAddressOf())));

        const auto st = scaler->run(dev.context.Get(), srv.Get(), srcRect, uav.Get(), dstRect, settings);
        REQUIRE_MESSAGE(st.ok(), st.error());

        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        REQUIRE(SUCCEEDED(dev.device->CreateTexture2D(&td, nullptr, staging.GetAddressOf())));
        dev.context->CopyResource(staging.Get(), dstTex.Get());
        D3D11_MAPPED_SUBRESOURCE m{};
        REQUIRE(SUCCEEDED(dev.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m)));
        std::vector<uint32_t> out(static_cast<size_t>(dstW) * dstH);
        for (uint32_t y = 0; y < dstH; ++y)
            std::memcpy(&out[static_cast<size_t>(y) * dstW], static_cast<const uint8_t*>(m.pData) + y * m.RowPitch,
                        dstW * 4);
        dev.context->Unmap(staging.Get(), 0);
        return out;
    }
};

uint32_t bgra(uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    return uint32_t{b} | (uint32_t{g} << 8) | (uint32_t{r} << 16) | (uint32_t{a} << 24);
}
// Same color as read back from an RGBA8 texture.
uint32_t rgba(uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    return uint32_t{r} | (uint32_t{g} << 8) | (uint32_t{b} << 16) | (uint32_t{a} << 24);
}

std::vector<uint32_t> pattern64() {
    std::vector<uint32_t> p(64 * 64);
    for (uint32_t y = 0; y < 64; ++y)
        for (uint32_t x = 0; x < 64; ++x)
            p[y * 64 + x] = bgra(static_cast<uint8_t>(x * 4), static_cast<uint8_t>(y * 4),
                                 static_cast<uint8_t>((x * 7 + y * 13) & 0xFF), 255);
    return p;
}

void checkOutsideUntouched(const std::vector<uint32_t>& out, uint32_t w, uint32_t h, Rect r) {
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const bool inside = x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
            if (!inside) {
                REQUIRE(out[y * w + x] == kSentinel);
            }
        }
}

int channelDiff(uint32_t a, uint32_t b) {
    int d = 0;
    for (int i = 0; i < 24; i += 8) // RGB only
        d = std::max(d, std::abs(static_cast<int>((a >> i) & 0xFF) - static_cast<int>((b >> i) & 0xFF)));
    return d;
}

} // namespace

TEST_CASE("d3d11: nearest reproduces exact 2x2 blocks") {
    Fixture f;
    if (!f.ok) return;
    const auto src = pattern64();
    const Rect full{0, 0, 128, 128};
    const auto out = f.run(src, 64, 64, {0, 0, 64, 64}, 128, 128, full, {ScaleAlgorithm::Nearest, 0.f});
    for (uint32_t y = 0; y < 128; ++y)
        for (uint32_t x = 0; x < 128; ++x) {
            const uint32_t s = src[(y / 2) * 64 + x / 2]; // BGRA bytes -> RGBA readback
            const uint32_t expect = rgba(s & 0xFF, (s >> 8) & 0xFF, (s >> 16) & 0xFF, (s >> 24) & 0xFF);
            REQUIRE(out[y * 128 + x] == expect);
        }
}

TEST_CASE("d3d11: bilinear of a constant image is constant") {
    Fixture f;
    if (!f.ok) return;
    const std::vector<uint32_t> src(64 * 64, bgra(40, 120, 200, 255));
    const auto out = f.run(src, 64, 64, {0, 0, 64, 64}, 128, 128, {0, 0, 128, 128}, {ScaleAlgorithm::Bilinear, 0.f});
    for (const uint32_t px : out) REQUIRE(channelDiff(px, rgba(40, 120, 200, 255)) <= 1);
}

TEST_CASE("d3d11: FSR1 of a constant image is constant") {
    Fixture f;
    if (!f.ok) return;
    for (const float sharp : {0.0f, 0.5f, 1.0f}) {
        const std::vector<uint32_t> src(64 * 64, bgra(40, 120, 200, 255));
        const auto out = f.run(src, 64, 64, {0, 0, 64, 64}, 128, 128, {0, 0, 128, 128}, {ScaleAlgorithm::Fsr1, sharp});
        for (const uint32_t px : out) REQUIRE(channelDiff(px, rgba(40, 120, 200, 255)) <= 1);
    }
}

TEST_CASE("d3d11: pixels outside dstRect are untouched") {
    Fixture f;
    if (!f.ok) return;
    const auto src = pattern64();
    const Rect dstRect{10, 21, 97, 83}; // deliberately not multiple of the threadgroup sizes
    for (const auto algo : {ScaleAlgorithm::Nearest, ScaleAlgorithm::Bilinear, ScaleAlgorithm::Fsr1}) {
        const auto out = f.run(src, 64, 64, {8, 4, 40, 50}, 128, 128, dstRect, {algo, 0.5f});
        checkOutsideUntouched(out, 128, 128, dstRect);
        // and the inside was written (sentinel has alpha 0xFF, G=0x12; the pattern pixel (0,0) differs)
        CHECK(out[dstRect.y * 128 + dstRect.x] != kSentinel);
    }
}

TEST_CASE("d3d11: invalid rectangles are rejected") {
    Fixture f;
    if (!f.ok) return;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = td.Height = 16;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> s;
    REQUIRE(SUCCEEDED(f.dev.device->CreateTexture2D(&td, nullptr, s.GetAddressOf())));
    ComPtr<ID3D11ShaderResourceView> srv;
    REQUIRE(SUCCEEDED(f.dev.device->CreateShaderResourceView(s.Get(), nullptr, srv.GetAddressOf())));
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> d;
    REQUIRE(SUCCEEDED(f.dev.device->CreateTexture2D(&td, nullptr, d.GetAddressOf())));
    ComPtr<ID3D11UnorderedAccessView> uav;
    REQUIRE(SUCCEEDED(f.dev.device->CreateUnorderedAccessView(d.Get(), nullptr, uav.GetAddressOf())));
    const auto st = f.scaler->run(f.dev.context.Get(), srv.Get(), {0, 0, 32, 32}, uav.Get(), {0, 0, 16, 16}, {});
    CHECK_FALSE(st.ok());
}

TEST_CASE("d3d11: GpuTimer reports scale time without blocking") {
    Fixture f;
    if (!f.ok) return;
    auto t = GpuTimer::create(f.dev.device.Get());
    REQUIRE_MESSAGE(t.ok(), t.error());
    auto timer = std::move(t).value();
    const auto src = pattern64();
    for (int i = 0; i < 20; ++i) {
        timer->beginFrame(f.dev.context.Get());
        timer->beginStage(f.dev.context.Get(), Stage::Scale);
        f.run(src, 64, 64, {0, 0, 64, 64}, 128, 128, {0, 0, 128, 128}, {ScaleAlgorithm::Fsr1, 0.5f});
        timer->endStage(f.dev.context.Get(), Stage::Scale);
        timer->endFrame(f.dev.context.Get());
    }
    const auto ms = timer->lastStageMs();
    CHECK(ms[static_cast<size_t>(Stage::Scale)] >= 0.0);
    CHECK(ms[static_cast<size_t>(Stage::Capture)] == 0.0);
}
