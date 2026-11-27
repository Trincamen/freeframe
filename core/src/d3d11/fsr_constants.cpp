#include "fsr_constants.hpp"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define A_CPU 1
#include "ffx_a.h"
#include "ffx_fsr1.h"

namespace ff::d3d11::detail {

void fsrEasuConstants(uint32_t out[16], float inViewW, float inViewH, float inTexW, float inTexH, float outW,
                      float outH, float inOffsetX, float inOffsetY) {
    FsrEasuConOffset(out, out + 4, out + 8, out + 12, inViewW, inViewH, inTexW, inTexH, outW, outH, inOffsetX,
                     inOffsetY);
}

void fsrRcasConstants(uint32_t out[4], float stops) { FsrRcasCon(out, stops); }

} // namespace ff::d3d11::detail
