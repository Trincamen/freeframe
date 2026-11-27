#pragma once
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "freeframe/d3d11/scaler.hpp"

namespace ffapp {

// Windows.Graphics.Capture of one window. Frames are polled (free-threaded pool) from the render thread
// and copied into a private texture so pool buffers are released immediately.
class CaptureWgc {
public:
    static std::unique_ptr<CaptureWgc> create(ID3D11Device* device, HWND target, std::string& error);
    ~CaptureWgc();
    CaptureWgc(const CaptureWgc&) = delete;
    CaptureWgc& operator=(const CaptureWgc&) = delete;

    // Pulls the newest frame, if any, into the private texture. Returns true when a new frame was copied.
    bool update(ID3D11DeviceContext* ctx);

    // Auto-reset event signaled by FrameArrived (capture thread) when a new frame is queued.
    HANDLE frameEvent() const noexcept;

    bool hasFrame() const noexcept { return m_hasFrame; }
    bool closed() const noexcept { return m_closed.load(); }
    ID3D11Texture2D* texture() const noexcept;
    ID3D11ShaderResourceView* srv() const noexcept;

    // Client-area rectangle inside the captured frame (clamped to the frame content). width==0 if unavailable.
    ff::d3d11::Rect clientRect() const;

private:
    CaptureWgc();
    struct Impl;
    std::unique_ptr<Impl> m;
    bool m_hasFrame = false;
    std::atomic<bool> m_closed{false};
};

} // namespace ffapp
