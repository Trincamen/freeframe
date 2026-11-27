// TEMPORARY stub so app-windows can link while the real D3D11 backend is written (M1).
// To be deleted by the core task that implements device.cpp / scaler.cpp / gpu_timer.cpp.
#include "freeframe/d3d11/scaler.hpp"

namespace ff::d3d11 {

Result<Device> createDevice(std::optional<LUID>, bool) {
    return Result<Device>::failure("d3d11 backend not implemented yet (stub)");
}

Result<std::unique_ptr<Scaler>> Scaler::create(ID3D11Device*) {
    return Result<std::unique_ptr<Scaler>>::failure("Scaler not implemented yet (stub)");
}

Result<std::unique_ptr<GpuTimer>> GpuTimer::create(ID3D11Device*) {
    return Result<std::unique_ptr<GpuTimer>>::failure("GpuTimer not implemented yet (stub)");
}

} // namespace ff::d3d11
