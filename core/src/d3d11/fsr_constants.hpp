#pragma once
// FSR1 constant setup on the CPU (wraps FsrEasuConOffset / FsrRcasCon from AMD's ffx_fsr1.h).
// Kept in its own translation unit: ffx_a.h defines many macros and must not meet <windows.h>.
#include <cstdint>

namespace ff::d3d11::detail {

// out: Const0..Const3 (4 uint32 each, 16 total).
void fsrEasuConstants(uint32_t out[16], float inViewW, float inViewH, float inTexW, float inTexH, float outW,
                      float outH, float inOffsetX, float inOffsetY);
// out: 4 uint32. `stops`: 0 = sharpest, N = N halvings of the sharpness.
void fsrRcasConstants(uint32_t out[4], float stops);

} // namespace ff::d3d11::detail
