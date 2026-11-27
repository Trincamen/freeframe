// freeframe-cli bench-scale (Windows only): GPU timing of the D3D11 scalers on a synthetic image.
#include "bench_scale.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "freeframe/d3d11/scaler.hpp"

namespace {

using namespace ff::d3d11;

struct Options {
    std::string algo = "all";
    uint32_t srcW = 1280, srcH = 720, dstW = 2560, dstH = 1440;
    int frames = 500;
    std::string dump;
};

bool parseSize(const char* s, uint32_t& w, uint32_t& h) {
    unsigned a = 0, b = 0;
    char tail = 0;
    if (std::sscanf(s, "%ux%u%c", &a, &b, &tail) != 2 || a == 0 || b == 0 || a > 16384 || b > 16384) return false;
    w = a;
    h = b;
    return true;
}

// BGRA test image: gradients, border, 1px checkerboard, diagonal lines and a pseudo-text block grid.
std::vector<uint32_t> makeImage(uint32_t w, uint32_t h) {
    std::vector<uint32_t> img(static_cast<size_t>(w) * h);
    auto pack = [](int r, int g, int b) {
        return uint32_t{static_cast<uint8_t>(b)} | (uint32_t{static_cast<uint8_t>(g)} << 8) |
               (uint32_t{static_cast<uint8_t>(r)} << 16) | 0xFF000000u;
    };
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            int r = static_cast<int>(x * 255 / w), g = static_cast<int>(y * 255 / h), b = 96;
            const uint32_t fx = x * 8 / w, fy = y * 8 / h;
            if (fx == 1 && fy == 1) { // fine checkerboard
                r = g = b = ((x + y) & 1) ? 235 : 20;
            } else if (fx == 2 && fy == 1) { // 2px stripes
                r = g = b = ((x / 2) & 1) ? 240 : 16;
            } else if (fx >= 4 && fx <= 6 && fy >= 3 && fy <= 5) { // text-like glyph grid
                const uint32_t cx = (x / 6) * 2654435761u, cy = (y / 9) * 40503u;
                const bool glyph = (((cx ^ cy) >> 5) & 3) != 0 && (x % 6) < 5 && (y % 9) < 7 &&
                                   ((((cx + cy * (x % 6 + 1)) >> (y % 9)) & 1) != 0 || (y % 9) == 3);
                r = g = b = glyph ? 10 : 245;
            }
            const int d = static_cast<int>(x) - static_cast<int>(y * w / h);
            if (std::abs(d) < 2) r = g = b = 255; // diagonal
            if (x < 3 || y < 3 || x + 3 >= w || y + 3 >= h) { r = 255; g = b = 0; } // border
            img[static_cast<size_t>(y) * w + x] = pack(r, g, b);
        }
    return img;
}

bool writeBmp(const std::string& path, const uint8_t* rgba, uint32_t w, uint32_t h, uint32_t pitch) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t rowBytes = (w * 3 + 3) & ~3u;
    const uint32_t dataSize = rowBytes * h;
    uint8_t hdr[54] = {'B', 'M'};
    auto put32 = [&](size_t off, uint32_t v) { std::memcpy(hdr + off, &v, 4); };
    auto put16 = [&](size_t off, uint16_t v) { std::memcpy(hdr + off, &v, 2); };
    put32(2, 54 + dataSize);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h); // positive: bottom-up
    put16(26, 1);
    put16(28, 24);
    put32(34, dataSize);
    bool ok = std::fwrite(hdr, 1, 54, f) == 54;
    std::vector<uint8_t> row(rowBytes, 0);
    for (uint32_t y = 0; y < h && ok; ++y) {
        const uint8_t* s = rgba + static_cast<size_t>(h - 1 - y) * pitch;
        for (uint32_t x = 0; x < w; ++x) {
            row[x * 3 + 0] = s[x * 4 + 2];
            row[x * 3 + 1] = s[x * 4 + 1];
            row[x * 3 + 2] = s[x * 4 + 0];
        }
        ok = std::fwrite(row.data(), 1, rowBytes, f) == rowBytes;
    }
    return std::fclose(f) == 0 && ok;
}

std::string withSuffix(const std::string& path, const char* suffix) {
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path + "_" + suffix;
    return path.substr(0, dot) + "_" + suffix + path.substr(dot);
}

const char* algoName(ScaleAlgorithm a) {
    switch (a) {
    case ScaleAlgorithm::Nearest: return "nearest";
    case ScaleAlgorithm::Bilinear: return "bilinear";
    case ScaleAlgorithm::Fsr1: return "fsr1";
    }
    return "?";
}

double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    const size_t i = std::min(sorted.size() - 1, static_cast<size_t>(p * static_cast<double>(sorted.size())));
    return sorted[i];
}

} // namespace

int cmdBenchScale(int argc, char** argv) {
    Options o;
    for (int i = 0; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        const char* v = nullptr;
        if (a == "--algo" && (v = next())) o.algo = v;
        else if (a == "--src" && (v = next()) && parseSize(v, o.srcW, o.srcH)) {}
        else if (a == "--dst" && (v = next()) && parseSize(v, o.dstW, o.dstH)) {}
        else if (a == "--frames" && (v = next())) o.frames = std::atoi(v);
        else if (a == "--dump" && (v = next())) o.dump = v;
        else {
            std::fprintf(stderr, "bench-scale: bad or incomplete argument \"%s\"\n", a.c_str());
            return 64;
        }
    }
    std::vector<ScaleAlgorithm> algos;
    if (o.algo == "nearest" || o.algo == "all") algos.push_back(ScaleAlgorithm::Nearest);
    if (o.algo == "bilinear" || o.algo == "all") algos.push_back(ScaleAlgorithm::Bilinear);
    if (o.algo == "fsr1" || o.algo == "all") algos.push_back(ScaleAlgorithm::Fsr1);
    if (algos.empty() || o.frames < 1) {
        std::fprintf(stderr, "bench-scale: --algo must be nearest|bilinear|fsr1|all and --frames >= 1\n");
        return 64;
    }

    auto dres = createDevice();
    if (!dres.ok()) {
        std::fprintf(stderr, "error: %s\n", dres.error().c_str());
        return 1;
    }
    Device dev = dres.value();
    DXGI_ADAPTER_DESC1 ad{};
    dev.adapter->GetDesc1(&ad);
    std::printf("Adapter: %ls\n", ad.Description);
    std::printf("Scale %ux%u -> %ux%u, %d frames\n", o.srcW, o.srcH, o.dstW, o.dstH, o.frames);

    auto sres = Scaler::create(dev.device.Get());
    if (!sres.ok()) {
        std::fprintf(stderr, "error: %s\n", sres.error().c_str());
        return 1;
    }
    auto scaler = std::move(sres).value();
    auto tres = GpuTimer::create(dev.device.Get());
    if (!tres.ok()) {
        std::fprintf(stderr, "error: %s\n", tres.error().c_str());
        return 1;
    }
    auto timer = std::move(tres).value();
    ID3D11DeviceContext* ctx = dev.context.Get();

    // Source texture (BGRA, like a capture) filled through UpdateSubresource.
    D3D11_TEXTURE2D_DESC td{};
    td.Width = o.srcW;
    td.Height = o.srcH;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> srcTex, dstTex, staging;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    HRESULT hr = dev.device->CreateTexture2D(&td, nullptr, srcTex.GetAddressOf());
    if (SUCCEEDED(hr)) hr = dev.device->CreateShaderResourceView(srcTex.Get(), nullptr, srv.GetAddressOf());
    if (FAILED(hr)) {
        std::fprintf(stderr, "error: creating source texture failed: HRESULT 0x%08X\n", static_cast<unsigned>(hr));
        return 1;
    }
    const auto img = makeImage(o.srcW, o.srcH);
    ctx->UpdateSubresource(srcTex.Get(), 0, nullptr, img.data(), o.srcW * 4, 0);

    td.Width = o.dstW;
    td.Height = o.dstH;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    hr = dev.device->CreateTexture2D(&td, nullptr, dstTex.GetAddressOf());
    if (SUCCEEDED(hr)) hr = dev.device->CreateUnorderedAccessView(dstTex.Get(), nullptr, uav.GetAddressOf());
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (SUCCEEDED(hr)) hr = dev.device->CreateTexture2D(&td, nullptr, staging.GetAddressOf());
    D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT, 0};
    ComPtr<ID3D11Query> evt;
    if (SUCCEEDED(hr)) hr = dev.device->CreateQuery(&qd, evt.GetAddressOf());
    if (FAILED(hr)) {
        std::fprintf(stderr, "error: creating destination resources failed: HRESULT 0x%08X\n", static_cast<unsigned>(hr));
        return 1;
    }

    // Each frame is fenced with an event query so every GpuTimer sample is read exactly once
    // (this benchmark may block; the timer itself never does).
    auto fence = [&] {
        ctx->End(evt.Get());
        ctx->Flush();
        while (ctx->GetData(evt.Get(), nullptr, 0, 0) == S_FALSE) {}
    };

    std::printf("\n%-10s %10s %10s %10s %10s\n", "algo", "avg ms", "p95 ms", "p99 ms", "max ms");
    for (const ScaleAlgorithm algo : algos) {
        const ScalerSettings settings{algo, 0.5f};
        const Rect srcRect{0, 0, o.srcW, o.srcH}, dstRect{0, 0, o.dstW, o.dstH};
        std::vector<double> samples;
        samples.reserve(static_cast<size_t>(o.frames));
        const int warmup = 30;
        for (int i = 0; i < warmup + o.frames + 1; ++i) {
            timer->beginFrame(ctx); // also polls the previous (fenced) frame
            if (i > warmup) samples.push_back(timer->lastStageMs()[static_cast<size_t>(Stage::Scale)]);
            if (i == warmup + o.frames) break; // final iteration only collects the last sample
            timer->beginStage(ctx, Stage::Scale);
            const auto st = scaler->run(ctx, srv.Get(), srcRect, uav.Get(), dstRect, settings);
            timer->endStage(ctx, Stage::Scale);
            timer->endFrame(ctx);
            if (!st.ok()) {
                std::fprintf(stderr, "error: %s\n", st.error().c_str());
                return 1;
            }
            fence();
        }
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        double sum = 0;
        for (const double s : samples) sum += s;
        std::printf("%-10s %10.3f %10.3f %10.3f %10.3f\n", algoName(algo), sum / static_cast<double>(samples.size()),
                    percentile(sorted, 0.95), percentile(sorted, 0.99), sorted.back());
        std::fflush(stdout);

        if (!o.dump.empty()) {
            ctx->CopyResource(staging.Get(), dstTex.Get());
            D3D11_MAPPED_SUBRESOURCE m{};
            hr = ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m);
            if (FAILED(hr)) {
                std::fprintf(stderr, "error: Map(staging) failed: HRESULT 0x%08X\n", static_cast<unsigned>(hr));
                return 1;
            }
            const std::string path = algos.size() > 1 ? withSuffix(o.dump, algoName(algo)) : o.dump;
            const bool ok = writeBmp(path, static_cast<const uint8_t*>(m.pData), o.dstW, o.dstH, m.RowPitch);
            ctx->Unmap(staging.Get(), 0);
            if (!ok) {
                std::fprintf(stderr, "error: could not write %s\n", path.c_str());
                return 1;
            }
            std::printf("  wrote %s\n", path.c_str());
        }
    }
    return 0;
}
