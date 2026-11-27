// Nearest / bilinear resampler. Compiled twice: USE_LINEAR=0 (point sampler) and USE_LINEAR=1.
// One thread per destination pixel of dstRect; dst pixel centers are mapped to source pixel centers.
cbuffer Params : register(b0) {
    uint2 dstOrigin; // dstRect.xy in the destination texture
    uint2 dstSize;   // dstRect.zw
    float4 srcRect;  // x, y, w, h in source pixels
};

Texture2D<float4> Src : register(t0);
RWTexture2D<unorm float4> Dst : register(u0);
SamplerState Smp : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= dstSize)) return;
    float2 texSize;
    Src.GetDimensions(texSize.x, texSize.y);
    float2 p = srcRect.xy + (float2(id.xy) + 0.5) * (srcRect.zw / float2(dstSize));
    // Keep taps inside srcRect (no bleeding from neighbouring regions of the source texture).
    p = clamp(p, srcRect.xy + 0.5, srcRect.xy + srcRect.zw - 0.5);
    Dst[dstOrigin + id.xy] = Src.SampleLevel(Smp, p / texSize, 0);
}
