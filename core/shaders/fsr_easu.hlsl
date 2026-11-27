// FSR 1 EASU (32-bit path), AMD FidelityFX (MIT). Writes RGBA8 intermediate of the dstRect size.
// Thread mapping as in the AMD sample: 64 threads, ARmp8x8 swizzle, each thread does 4 pixels of a 16x16 tile.
cbuffer Params : register(b0) {
    uint4 Const0;
    uint4 Const1;
    uint4 Const2;
    uint4 Const3;
    uint2 dstSize;
    uint2 pad;
};

#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

Texture2D<float4> InputTexture : register(t0);
RWTexture2D<unorm float4> OutputTexture : register(u0);
SamplerState samLinearClamp : register(s0);

#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return InputTexture.GatherRed(samLinearClamp, p, int2(0, 0)); }
AF4 FsrEasuGF(AF2 p) { return InputTexture.GatherGreen(samLinearClamp, p, int2(0, 0)); }
AF4 FsrEasuBF(AF2 p) { return InputTexture.GatherBlue(samLinearClamp, p, int2(0, 0)); }
#include "ffx_fsr1.h"

void Filter(AU2 pos) {
    if (any(pos >= dstSize)) return;
    AF3 c;
    FsrEasuF(c, pos, Const0, Const1, Const2, Const3);
    OutputTexture[pos] = float4(c, 1.0);
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
