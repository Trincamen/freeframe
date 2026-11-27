#include <exception>

#include "util.hpp"

namespace ff::d3d11 {

namespace {

constexpr size_t kRing = 4;
constexpr size_t kStages = static_cast<size_t>(Stage::Count);

struct FrameSlot {
    ComPtr<ID3D11Query> disjoint;
    ComPtr<ID3D11Query> begin[kStages];
    ComPtr<ID3D11Query> end[kStages];
    bool pending = false;   // submitted, results not read yet
    bool recording = false; // between beginFrame and endFrame
    bool used[kStages]{};   // stage fully recorded this frame
    bool open[kStages]{};   // beginStage issued, endStage not yet
};

class GpuTimerImpl final : public GpuTimer {
public:
    Status init(ID3D11Device* dev) {
        const D3D11_QUERY_DESC dq{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        const D3D11_QUERY_DESC tq{D3D11_QUERY_TIMESTAMP, 0};
        for (auto& s : m_slots) {
            HRESULT hr = dev->CreateQuery(&dq, s.disjoint.GetAddressOf());
            if (FAILED(hr)) return detail::failStatus("CreateQuery(TIMESTAMP_DISJOINT)", hr);
            for (size_t i = 0; i < kStages; ++i) {
                hr = dev->CreateQuery(&tq, s.begin[i].GetAddressOf());
                if (FAILED(hr)) return detail::failStatus("CreateQuery(TIMESTAMP)", hr);
                hr = dev->CreateQuery(&tq, s.end[i].GetAddressOf());
                if (FAILED(hr)) return detail::failStatus("CreateQuery(TIMESTAMP)", hr);
            }
        }
        return okStatus();
    }

    void beginFrame(ID3D11DeviceContext* ctx) override {
        poll(ctx);
        FrameSlot& s = m_slots[m_cur];
        if (s.pending) { // ring full: the GPU is too far behind, skip measuring this frame
            m_active = nullptr;
            return;
        }
        for (size_t i = 0; i < kStages; ++i) s.used[i] = s.open[i] = false;
        ctx->Begin(s.disjoint.Get());
        s.recording = true;
        m_active = &s;
    }

    void beginStage(ID3D11DeviceContext* ctx, Stage stage) override {
        const size_t i = static_cast<size_t>(stage);
        if (!m_active || i >= kStages) return;
        ctx->End(m_active->begin[i].Get());
        m_active->open[i] = true;
    }

    void endStage(ID3D11DeviceContext* ctx, Stage stage) override {
        const size_t i = static_cast<size_t>(stage);
        if (!m_active || i >= kStages || !m_active->open[i]) return;
        ctx->End(m_active->end[i].Get());
        m_active->open[i] = false;
        m_active->used[i] = true;
    }

    void endFrame(ID3D11DeviceContext* ctx) override {
        if (!m_active) return;
        ctx->End(m_active->disjoint.Get());
        m_active->recording = false;
        m_active->pending = true;
        m_active = nullptr;
        m_cur = (m_cur + 1) % kRing;
        poll(ctx);
    }

    std::array<double, kStages> lastStageMs() const override { return m_last; }

private:
    // Reads finished frames, oldest first (m_cur is the next slot to be written, i.e. the oldest one).
    // Never flushes and never blocks.
    void poll(ID3D11DeviceContext* ctx) {
        for (size_t n = 0; n < kRing; ++n) {
            FrameSlot& s = m_slots[(m_cur + n) % kRing];
            if (!s.pending) continue;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
            if (ctx->GetData(s.disjoint.Get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) return;
            std::array<double, kStages> ms{};
            const bool good = !dj.Disjoint && dj.Frequency != 0;
            for (size_t i = 0; i < kStages && good; ++i) {
                if (!s.used[i]) continue;
                UINT64 t0 = 0, t1 = 0;
                if (ctx->GetData(s.begin[i].Get(), &t0, sizeof(t0), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
                    ctx->GetData(s.end[i].Get(), &t1, sizeof(t1), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
                    return; // not ready yet; keep pending
                }
                ms[i] = t1 >= t0 ? static_cast<double>(t1 - t0) * 1000.0 / static_cast<double>(dj.Frequency) : 0.0;
            }
            s.pending = false;
            if (good) m_last = ms;
        }
    }

    FrameSlot m_slots[kRing];
    FrameSlot* m_active = nullptr;
    size_t m_cur = 0;
    std::array<double, kStages> m_last{};
};

} // namespace

Result<std::unique_ptr<GpuTimer>> GpuTimer::create(ID3D11Device* device) {
    using R = Result<std::unique_ptr<GpuTimer>>;
    if (!device) return R::failure("GpuTimer::create: null device");
    try {
        auto t = std::make_unique<GpuTimerImpl>();
        const Status st = t->init(device);
        if (!st.ok()) return R::failure(st.error());
        return R::success(std::move(t));
    } catch (const std::exception& e) {
        return R::failure(std::string("GpuTimer::create: ") + e.what());
    }
}

} // namespace ff::d3d11
