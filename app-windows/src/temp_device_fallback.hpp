#pragma once
// TEMP(M1-S2): remover após integração do S1 (core d3d11 backend).
#include "freeframe/d3d11/scaler.hpp"

namespace ffapp {

// Plain D3D11CreateDevice on the default adapter with BGRA support.
ff::Result<ff::d3d11::Device> tempCreateDeviceFallback();

// Unscaled copy of srcRect centered in dst (clamped to dst size). src/dst must share a format family.
void tempCopyCentered(ID3D11DeviceContext* ctx, ID3D11Texture2D* src, ff::d3d11::Rect srcRect, ID3D11Texture2D* dst);

} // namespace ffapp
