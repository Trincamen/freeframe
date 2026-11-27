#include "capture_wgc.hpp"

#include <dwmapi.h>
#include <inspectable.h>
#include <unknwn.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <algorithm>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/base.h>

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgd = winrt::Windows::Graphics::DirectX;
namespace wgd11 = winrt::Windows::Graphics::DirectX::Direct3D11;

namespace {
// Same IID as Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess.
struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDxgiInterfaceAccess : ::IUnknown {
    virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
};
} // namespace

namespace ffapp {

struct CaptureWgc::Impl {
    HWND target = nullptr;
    winrt::com_ptr<ID3D11Device> device;
    wgd11::IDirect3DDevice winrtDevice{nullptr};
    wgc::GraphicsCaptureItem item{nullptr};
    wgc::Direct3D11CaptureFramePool pool{nullptr};
    wgc::GraphicsCaptureSession session{nullptr};
    winrt::event_token closedToken{};
    winrt::Windows::Graphics::SizeInt32 poolSize{};
    winrt::Windows::Graphics::SizeInt32 contentSize{};
    winrt::com_ptr<ID3D11Texture2D> copy;
    winrt::com_ptr<ID3D11ShaderResourceView> copySrv;
    UINT copyW = 0, copyH = 0;
};

CaptureWgc::CaptureWgc() : m(std::make_unique<Impl>()) {}

CaptureWgc::~CaptureWgc() {
    try {
        if (m->item && m->closedToken.value) m->item.Closed(m->closedToken);
        if (m->session) m->session.Close();
        if (m->pool) m->pool.Close();
    } catch (...) {
    }
}

std::unique_ptr<CaptureWgc> CaptureWgc::create(ID3D11Device* device, HWND target, std::string& error) {
    try {
        if (!wgc::GraphicsCaptureSession::IsSupported()) {
            error = "Windows.Graphics.Capture is not supported on this system";
            return nullptr;
        }
        std::unique_ptr<CaptureWgc> c(new CaptureWgc());
        Impl& s = *c->m;
        s.target = target;
        s.device.copy_from(device);

        winrt::com_ptr<IDXGIDevice> dxgi = s.device.as<IDXGIDevice>();
        winrt::com_ptr<::IInspectable> insp;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), insp.put()));
        s.winrtDevice = insp.as<wgd11::IDirect3DDevice>();

        auto interop = winrt::get_activation_factory<wgc::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(interop->CreateForWindow(target, winrt::guid_of<wgc::GraphicsCaptureItem>(),
                                                      winrt::put_abi(s.item)));
        s.contentSize = s.item.Size();
        s.poolSize = s.contentSize;
        CaptureWgc* raw = c.get();
        s.closedToken = s.item.Closed([raw](auto&&, auto&&) { raw->m_closed.store(true); });

        s.pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            s.winrtDevice, wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, s.poolSize);
        s.session = s.pool.CreateCaptureSession(s.item);

        using winrt::Windows::Foundation::Metadata::ApiInformation;
        if (ApiInformation::IsPropertyPresent(L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsBorderRequired"))
            s.session.IsBorderRequired(false);
        if (ApiInformation::IsPropertyPresent(L"Windows.Graphics.Capture.GraphicsCaptureSession",
                                              L"IsCursorCaptureEnabled"))
            s.session.IsCursorCaptureEnabled(false);
        s.session.StartCapture();
        return c;
    } catch (const winrt::hresult_error& e) {
        char buf[128];
        snprintf(buf, sizeof buf, "WGC setup failed (0x%08lX)", static_cast<unsigned long>(e.code().value));
        error = buf;
    } catch (const std::exception& e) {
        error = std::string("WGC setup failed: ") + e.what();
    }
    return nullptr;
}

bool CaptureWgc::update(ID3D11DeviceContext* ctx) {
    Impl& s = *m;
    bool got = false;
    try {
        // Drain the pool, keeping only the newest frame.
        wgc::Direct3D11CaptureFrame frame{nullptr};
        while (auto next = s.pool.TryGetNextFrame()) frame = next;
        if (!frame) return false;

        const auto size = frame.ContentSize();
        s.contentSize = size;
        auto access = frame.Surface().as<IDxgiInterfaceAccess>();
        winrt::com_ptr<ID3D11Texture2D> tex;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), tex.put_void()));
        D3D11_TEXTURE2D_DESC desc{};
        tex->GetDesc(&desc);

        if (!s.copy || s.copyW != desc.Width || s.copyH != desc.Height) {
            D3D11_TEXTURE2D_DESC cd = desc;
            cd.MipLevels = 1;
            cd.ArraySize = 1;
            cd.SampleDesc = {1, 0};
            cd.Usage = D3D11_USAGE_DEFAULT;
            cd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            cd.CPUAccessFlags = 0;
            cd.MiscFlags = 0;
            s.copySrv = nullptr;
            s.copy = nullptr;
            winrt::check_hresult(s.device->CreateTexture2D(&cd, nullptr, s.copy.put()));
            winrt::check_hresult(s.device->CreateShaderResourceView(s.copy.get(), nullptr, s.copySrv.put()));
            s.copyW = desc.Width;
            s.copyH = desc.Height;
        }
        ctx->CopyResource(s.copy.get(), tex.get());
        m_hasFrame = true;
        got = true;

        if (size.Width != s.poolSize.Width || size.Height != s.poolSize.Height) {
            s.poolSize = size;
            s.pool.Recreate(s.winrtDevice, wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
        }
    } catch (const winrt::hresult_error&) {
        // Transient (e.g. surface lost during resize); keep the last good frame.
    }
    return got;
}

ID3D11Texture2D* CaptureWgc::texture() const noexcept { return m->copy.get(); }
ID3D11ShaderResourceView* CaptureWgc::srv() const noexcept { return m->copySrv.get(); }

ff::d3d11::Rect CaptureWgc::clientRect() const {
    const Impl& s = *m;
    RECT ext{}, cr{};
    POINT origin{0, 0};
    if (FAILED(DwmGetWindowAttribute(s.target, DWMWA_EXTENDED_FRAME_BOUNDS, &ext, sizeof ext)) ||
        !GetClientRect(s.target, &cr) || !ClientToScreen(s.target, &origin))
        return {};
    const LONG cw = std::min<LONG>(s.contentSize.Width, static_cast<LONG>(s.copyW));
    const LONG ch = std::min<LONG>(s.contentSize.Height, static_cast<LONG>(s.copyH));
    LONG x = std::clamp<LONG>(origin.x - ext.left, 0, cw);
    LONG y = std::clamp<LONG>(origin.y - ext.top, 0, ch);
    LONG w = std::min<LONG>(cr.right - cr.left, cw - x);
    LONG h = std::min<LONG>(cr.bottom - cr.top, ch - y);
    if (w <= 0 || h <= 0) return {};
    return {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

} // namespace ffapp
