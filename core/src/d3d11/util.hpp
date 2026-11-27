#pragma once
// Internal helpers for the D3D11 backend (not part of the public contract).
#include <cstdio>
#include <string>

#include "freeframe/d3d11/scaler.hpp"

namespace ff::d3d11::detail {

inline std::string hrHex(HRESULT hr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(hr));
    return buf;
}

inline std::string hrMessage(const char* what, HRESULT hr) {
    return std::string(what) + " failed: HRESULT " + hrHex(hr);
}

template <typename T>
Result<T> fail(const char* what, HRESULT hr) {
    return Result<T>::failure(hrMessage(what, hr));
}

inline Status failStatus(const char* what, HRESULT hr) { return Status::failure(hrMessage(what, hr)); }

} // namespace ff::d3d11::detail
