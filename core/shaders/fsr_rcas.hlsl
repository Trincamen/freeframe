// FSR 1 RCAS (32-bit path), AMD FidelityFX (MIT). Reads the EASU intermediate, writes into dst at dstOrigin.
cbuffer Params : register(b0) {
    uint4 Const0;
    uint2 dstOrigin;
    uint2 dstSize;
};

#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

Texture2D<float4> InputTexture : register(t0);
RWTexture2D<unorm float4> OutputTexture : register(u0);

#define FSR_RCAS_F 1
// Clamp to the intermediate: Load outside the texture returns 0 and would darken the borders.
AF4 FsrRcasLoadF(ASU2 p) { return InputTexture.Load(int3(clamp(p, ASU2(0, 0), ASU2(dstSize) - 1), 0)); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "ffx_fsr1.h"

void Filter(AU2 pos) {
    if (any(pos >= dstSize)) return;
    AF3 c;
    FsrRcasF(c.r, c.g, c.b, pos, Const0);
    OutputTexture[dstOrigin + pos] = float4(c, 1.0);
}

[numthreads(64, 1, 1)]
void main(uint3 localId : SV_GroupThreadID, uint3 groupId : SV_GroupID) {
    AU2 gxy = ARmp8x8(localId.x) + AU2(groupId.x << 4u, groupId.y << 4u);
    Filter(gxy);
    gxy.x += 8u;
    Filter(gxy);
    gxy.y += 8u;
    Filter(gxy);
    gxy.x -= 8u;
    Filter(gxy);
}
