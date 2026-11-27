#include <doctest/doctest.h>

#include "freeframe/recommendations.hpp"

#include <string>
#include <vector>

namespace {

ff::SessionContext makeCtx(uint32_t srcW, uint32_t srcH, uint32_t dstW, uint32_t dstH) {
    ff::SessionContext c;
    c.srcWidth = srcW;
    c.srcHeight = srcH;
    c.dstWidth = dstW;
    c.dstHeight = dstH;
    return c;
}

bool hasId(const std::vector<ff::Tip>& tips, const std::string& id) {
    for (const auto& t : tips) {
        if (t.id == id) return true;
    }
    return false;
}

const ff::Tip* findId(const std::vector<ff::Tip>& tips, const std::string& id) {
    for (const auto& t : tips) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("scale.none fires when source equals output resolution") {
    const auto tips = ff::recommend(makeCtx(2560, 1440, 2560, 1440));
    const ff::Tip* t = findId(tips, "scale.none");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Warning);
    CHECK(contains(t->text, "1706x960"));
}

TEST_CASE("scale.none does not fire for a real upscale") {
    const auto tips = ff::recommend(makeCtx(1280, 720, 2560, 1440));
    CHECK_FALSE(hasId(tips, "scale.none"));
}

TEST_CASE("scale.too_aggressive fires above 1.55x with FSR 1") {
    const auto tips = ff::recommend(makeCtx(1280, 720, 2560, 1440));
    const ff::Tip* t = findId(tips, "scale.too_aggressive");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Suggestion);
    CHECK(contains(t->text, "2.00x"));
    CHECK(contains(t->text, "1706x960"));
    CHECK(contains(t->text, "1920x1080"));
}

TEST_CASE("scale.too_aggressive fires with bilinear") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.algorithm = ff::AlgorithmKind::Bilinear;
    CHECK(hasId(ff::recommend(ctx), "scale.too_aggressive"));
}

TEST_CASE("scale.too_aggressive does not fire at 1.33x") {
    const auto tips = ff::recommend(makeCtx(1920, 1080, 2560, 1440));
    CHECK_FALSE(hasId(tips, "scale.too_aggressive"));
}

TEST_CASE("scale.too_aggressive does not fire for nearest") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.algorithm = ff::AlgorithmKind::Nearest;
    CHECK_FALSE(hasId(ff::recommend(ctx), "scale.too_aggressive"));
}

TEST_CASE("sharpness.low fires for FSR 1 at 1.4x+ with low sharpness") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.sharpness = 0.2f;
    const auto tips = ff::recommend(ctx);
    const ff::Tip* t = findId(tips, "sharpness.low");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Suggestion);
    CHECK(contains(t->text, "2.00x"));
}

TEST_CASE("sharpness.low does not fire with normal sharpness") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.sharpness = 0.5f;
    CHECK_FALSE(hasId(ff::recommend(ctx), "sharpness.low"));
}

TEST_CASE("sharpness.low does not fire below 1.4x") {
    auto ctx = makeCtx(1600, 1200, 1920, 1440); // factor 1.2
    ctx.sharpness = 0.1f;
    CHECK_FALSE(hasId(ff::recommend(ctx), "sharpness.low"));
}

TEST_CASE("sharpness.high fires above 0.9 with FSR 1") {
    auto ctx = makeCtx(1920, 1080, 2560, 1440);
    ctx.sharpness = 0.95f;
    const auto tips = ff::recommend(ctx);
    const ff::Tip* t = findId(tips, "sharpness.high");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Info);
}

TEST_CASE("sharpness.high does not fire at 0.5") {
    auto ctx = makeCtx(1920, 1080, 2560, 1440);
    ctx.sharpness = 0.5f;
    CHECK_FALSE(hasId(ff::recommend(ctx), "sharpness.high"));
}

TEST_CASE("nearest.non_integer fires for nearest at 1.5x") {
    auto ctx = makeCtx(1280, 720, 1920, 1080);
    ctx.algorithm = ff::AlgorithmKind::Nearest;
    const auto tips = ff::recommend(ctx);
    const ff::Tip* t = findId(tips, "nearest.non_integer");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Suggestion);
    CHECK(contains(t->text, "1.50x"));
    CHECK(contains(t->text, "960x540")); // N < 2 -> use 2x
    CHECK(contains(t->text, "for 2x"));
}

TEST_CASE("nearest.non_integer does not fire for an integer factor") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.algorithm = ff::AlgorithmKind::Nearest;
    CHECK_FALSE(hasId(ff::recommend(ctx), "nearest.non_integer"));
}

TEST_CASE("nearest.non_integer does not fire for FSR 1") {
    auto ctx = makeCtx(1280, 720, 1920, 1080);
    ctx.algorithm = ff::AlgorithmKind::Fsr1;
    CHECK_FALSE(hasId(ff::recommend(ctx), "nearest.non_integer"));
}

TEST_CASE("fps.cpu_bound fires when lowering resolution did not raise FPS") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.baseFps = 56.0;
    ctx.higherResBaseFps = 55.0;
    ctx.higherResSrcHeight = 1440u;
    const auto tips = ff::recommend(ctx);
    const ff::Tip* t = findId(tips, "fps.cpu_bound");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Warning);
    CHECK(contains(t->text, "1440p to 720p"));
    CHECK(contains(t->text, "55"));
    CHECK(contains(t->text, "56 fps"));
}

TEST_CASE("fps.cpu_bound does not fire when FPS rose clearly") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.baseFps = 70.0;
    ctx.higherResBaseFps = 55.0;
    ctx.higherResSrcHeight = 1440u;
    CHECK_FALSE(hasId(ff::recommend(ctx), "fps.cpu_bound"));
}

TEST_CASE("fps.cpu_bound does not fire without a higher resolution") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.baseFps = 56.0;
    ctx.higherResBaseFps = 55.0;
    ctx.higherResSrcHeight = 720u; // not higher than the current source
    CHECK_FALSE(hasId(ff::recommend(ctx), "fps.cpu_bound"));
}

TEST_CASE("fg.low_base_fps fires below 30 fps with frame generation") {
    auto ctx = makeCtx(1920, 1080, 2560, 1440);
    ctx.frameGenEnabled = true;
    ctx.baseFps = 25.0;
    const auto tips = ff::recommend(ctx);
    const ff::Tip* t = findId(tips, "fg.low_base_fps");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Warning);
}

TEST_CASE("fg.low_base_fps does not fire at 45 fps") {
    auto ctx = makeCtx(1920, 1080, 2560, 1440);
    ctx.frameGenEnabled = true;
    ctx.baseFps = 45.0;
    CHECK_FALSE(hasId(ff::recommend(ctx), "fg.low_base_fps"));
}

TEST_CASE("fg.low_base_fps does not fire without frame generation") {
    auto ctx = makeCtx(1920, 1080, 2560, 1440);
    ctx.frameGenEnabled = false;
    ctx.baseFps = 25.0;
    CHECK_FALSE(hasId(ff::recommend(ctx), "fg.low_base_fps"));
}

TEST_CASE("game.native_upscaler fires when the game has its own upscaler") {
    auto ctx = makeCtx(1920, 1080, 2560, 1440);
    ctx.gameHasNativeUpscaler = true;
    const auto tips = ff::recommend(ctx);
    const ff::Tip* t = findId(tips, "game.native_upscaler");
    REQUIRE(t != nullptr);
    CHECK(t->severity == ff::TipSeverity::Info);
}

TEST_CASE("game.native_upscaler does not fire for nearest or unknown games") {
    auto nearest = makeCtx(1920, 1080, 2560, 1440);
    nearest.gameHasNativeUpscaler = true;
    nearest.algorithm = ff::AlgorithmKind::Nearest;
    CHECK_FALSE(hasId(ff::recommend(nearest), "game.native_upscaler"));

    auto unknown = makeCtx(1920, 1080, 2560, 1440);
    unknown.gameHasNativeUpscaler = std::nullopt;
    CHECK_FALSE(hasId(ff::recommend(unknown), "game.native_upscaler"));
}

TEST_CASE("recommend orders Warning, then Suggestion, then Info") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.frameGenEnabled = true;
    ctx.baseFps = 25.0;
    ctx.sharpness = 0.95f;
    const auto tips = ff::recommend(ctx);
    REQUIRE(tips.size() == 3);
    CHECK(tips[0].id == "fg.low_base_fps");
    CHECK(tips[0].severity == ff::TipSeverity::Warning);
    CHECK(tips[1].id == "scale.too_aggressive");
    CHECK(tips[1].severity == ff::TipSeverity::Suggestion);
    CHECK(tips[2].id == "sharpness.high");
    CHECK(tips[2].severity == ff::TipSeverity::Info);
}

TEST_CASE("recommend: real user case 1280x720 -> 2560x1440 (CPU-bound + aggressive scale)") {
    auto ctx = makeCtx(1280, 720, 2560, 1440);
    ctx.algorithm = ff::AlgorithmKind::Fsr1;
    ctx.sharpness = 0.5f;
    ctx.baseFps = 56.0;
    ctx.higherResBaseFps = 55.0;
    ctx.higherResSrcHeight = 1440u;
    const auto tips = ff::recommend(ctx);
    REQUIRE(tips.size() == 2);
    CHECK(tips[0].id == "fps.cpu_bound");
    CHECK(tips[0].severity == ff::TipSeverity::Warning);
    CHECK(tips[1].id == "scale.too_aggressive");
    CHECK(contains(tips[1].text, "1706x960"));
    CHECK(contains(tips[1].text, "1920x1080"));
}

TEST_CASE("recommend returns nothing for an all-zero context") {
    const ff::SessionContext ctx;
    CHECK(ff::recommend(ctx).empty());
}

TEST_CASE("to_string for TipSeverity") {
    CHECK(std::string(ff::to_string(ff::TipSeverity::Info)) == "info");
    CHECK(std::string(ff::to_string(ff::TipSeverity::Suggestion)) == "suggestion");
    CHECK(std::string(ff::to_string(ff::TipSeverity::Warning)) == "warning");
}
