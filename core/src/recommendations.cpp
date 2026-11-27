#include "freeframe/recommendations.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ff {

namespace {

// Rounds a positive pixel size to the nearest even number (game resolutions in the tips).
uint32_t roundToEven(double value) {
    const long long halves = std::llround(value / 2.0);
    return static_cast<uint32_t>(halves * 2);
}

bool isScaleKnown(const SessionContext& ctx) {
    return ctx.srcWidth != 0 && ctx.srcHeight != 0 && ctx.dstWidth != 0 && ctx.dstHeight != 0;
}

} // namespace

std::vector<Tip> recommend(const SessionContext& ctx) {
    std::vector<Tip> tips;
    char buf[320];

    const bool scaleKnown = isScaleKnown(ctx);
    const double factor = scaleKnown
        ? static_cast<double>(ctx.dstHeight) / static_cast<double>(ctx.srcHeight)
        : 0.0;

    // 1. scale.none (Warning)
    if (scaleKnown && ctx.srcWidth == ctx.dstWidth && ctx.srcHeight == ctx.dstHeight) {
        const uint32_t w = roundToEven(static_cast<double>(ctx.dstWidth) / 1.5);
        const uint32_t h = roundToEven(static_cast<double>(ctx.dstHeight) / 1.5);
        std::snprintf(buf, sizeof(buf),
            "No upscaling: the game already renders at the output resolution. "
            "Run it windowed at a lower resolution (e.g. %ux%u for 1.5x).",
            static_cast<unsigned>(w), static_cast<unsigned>(h));
        tips.push_back({"scale.none", TipSeverity::Warning, buf});
    }

    // 2. scale.too_aggressive (Suggestion)
    if (scaleKnown && factor > 1.55 &&
        (ctx.algorithm == AlgorithmKind::Fsr1 || ctx.algorithm == AlgorithmKind::Bilinear)) {
        const uint32_t aw = roundToEven(static_cast<double>(ctx.dstWidth) / 1.5);
        const uint32_t ah = roundToEven(static_cast<double>(ctx.dstHeight) / 1.5);
        const uint32_t cw = roundToEven(static_cast<double>(ctx.dstWidth) / (4.0 / 3.0));
        const uint32_t ch = roundToEven(static_cast<double>(ctx.dstHeight) / (4.0 / 3.0));
        std::snprintf(buf, sizeof(buf),
            "Upscale factor %.2fx is aggressive: fine detail (hair, foliage, wires) will look rough. "
            "For better quality set the game to %ux%u (1.5x) or %ux%u (1.33x).",
            factor, static_cast<unsigned>(aw), static_cast<unsigned>(ah),
            static_cast<unsigned>(cw), static_cast<unsigned>(ch));
        tips.push_back({"scale.too_aggressive", TipSeverity::Suggestion, buf});
    }

    // 3. sharpness.low (Suggestion)
    if (scaleKnown && ctx.algorithm == AlgorithmKind::Fsr1 && factor >= 1.4 &&
        static_cast<double>(ctx.sharpness) < 0.35) {
        std::snprintf(buf, sizeof(buf),
            "Sharpness is low for a %.2fx upscale; try 0.6-0.8 (Ctrl+Alt+Up).", factor);
        tips.push_back({"sharpness.low", TipSeverity::Suggestion, buf});
    }

    // 4. sharpness.high (Info)
    if (ctx.algorithm == AlgorithmKind::Fsr1 && static_cast<double>(ctx.sharpness) > 0.9) {
        tips.push_back({"sharpness.high", TipSeverity::Info,
            "Very high sharpness can add halos and shimmering; 0.6-0.8 is usually a good balance "
            "(Ctrl+Alt+Down)."});
    }

    // 5. nearest.non_integer (Suggestion)
    if (scaleKnown && ctx.algorithm == AlgorithmKind::Nearest &&
        std::fabs(factor - std::round(factor)) > 0.01) {
        int n = static_cast<int>(std::floor(factor));
        if (n < 2) n = 2;
        const uint32_t w = roundToEven(static_cast<double>(ctx.dstWidth) / n);
        const uint32_t h = roundToEven(static_cast<double>(ctx.dstHeight) / n);
        std::snprintf(buf, sizeof(buf),
            "Nearest with a non-integer factor (%.2fx) gives uneven pixels. Use an integer factor "
            "(e.g. game at %ux%u for %dx) or FSR 1 for 3D games (Ctrl+Alt+A).",
            factor, static_cast<unsigned>(w), static_cast<unsigned>(h), n);
        tips.push_back({"nearest.non_integer", TipSeverity::Suggestion, buf});
    }

    // 6. fps.cpu_bound (Warning)
    if (ctx.higherResBaseFps && ctx.higherResSrcHeight &&
        *ctx.higherResSrcHeight > ctx.srcHeight && ctx.baseFps > 0.0 &&
        ctx.baseFps <= *ctx.higherResBaseFps * 1.10) {
        std::snprintf(buf, sizeof(buf),
            "Lowering the resolution from %up to %up did not raise FPS (%.0f -> %.0f fps): "
            "the game is CPU-limited or FPS-capped. Upscaling will not increase FPS here; "
            "play at native resolution for best quality, or use frame generation.",
            static_cast<unsigned>(*ctx.higherResSrcHeight), static_cast<unsigned>(ctx.srcHeight),
            *ctx.higherResBaseFps, ctx.baseFps);
        tips.push_back({"fps.cpu_bound", TipSeverity::Warning, buf});
    }

    // 7. fg.low_base_fps (Warning)
    if (ctx.frameGenEnabled && ctx.baseFps > 0.0 && ctx.baseFps < 30.0) {
        tips.push_back({"fg.low_base_fps", TipSeverity::Warning,
            "Base FPS below 30 causes visible frame-generation artifacts; lower game settings or "
            "resolution to reach 40+ fps."});
    }

    // 8. game.native_upscaler (Info)
    if (ctx.gameHasNativeUpscaler && *ctx.gameHasNativeUpscaler &&
        ctx.algorithm != AlgorithmKind::Nearest) {
        tips.push_back({"game.native_upscaler", TipSeverity::Info,
            "This game has its own upscaler (DLSS/FSR/XeSS/TSR); it usually looks better than "
            "spatial upscaling. FreeFrame is still useful for frame generation."});
    }

    // Warning first, then Suggestion, then Info; stable keeps the rule order within a severity.
    std::stable_sort(tips.begin(), tips.end(), [](const Tip& a, const Tip& b) {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });
    return tips;
}

const char* to_string(TipSeverity s) noexcept {
    switch (s) {
    case TipSeverity::Info: return "info";
    case TipSeverity::Suggestion: return "suggestion";
    case TipSeverity::Warning: return "warning";
    }
    return "?";
}

} // namespace ff
