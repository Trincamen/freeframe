#pragma once
// Contextual recommendations engine (PROJETO.md section 7D). Pure, platform-independent, unit-tested.
// Contract between core (implementation) and UIs (overlay, profile screens, in-game panel, stats).
// Changing this header requires the orchestrator's approval.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ff {

enum class TipSeverity { Info, Suggestion, Warning };

struct Tip {
    std::string id;   // stable identifier, e.g. "scale.too_aggressive" (used for i18n and "don't show again")
    TipSeverity severity = TipSeverity::Info;
    std::string text; // English for now (i18n later, keyed by id)
};

enum class AlgorithmKind { Nearest, Bilinear, Fsr1, Other };

struct SessionContext {
    uint32_t srcWidth = 0, srcHeight = 0; // captured game client area
    uint32_t dstWidth = 0, dstHeight = 0; // output rectangle (aspect-fit inside the monitor)
    uint32_t monitorWidth = 0, monitorHeight = 0;
    AlgorithmKind algorithm = AlgorithmKind::Fsr1;
    float sharpness = 0.5f;               // 0..1
    double baseFps = 0.0;                 // real frames per second produced by the game (current)
    // Average base FPS previously measured for the same game at a HIGHER source resolution, if known
    // (e.g. before the user lowered the in-game resolution). Lets us detect CPU-bound / capped games.
    std::optional<double> higherResBaseFps;
    std::optional<uint32_t> higherResSrcHeight; // the source height that `higherResBaseFps` was measured at
    bool frameGenEnabled = false;
    std::optional<bool> gameHasNativeUpscaler; // known from the profile (DLSS/FSR2+/XeSS/TSR), unknown otherwise
};

// Returns tips ordered by importance (Warning first). Deterministic; no side effects.
std::vector<Tip> recommend(const SessionContext& ctx);

const char* to_string(TipSeverity s) noexcept;

} // namespace ff
