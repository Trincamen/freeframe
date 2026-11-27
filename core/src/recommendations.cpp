// STUB — to be replaced by the real rules (M1, recommendations task).
#include "freeframe/recommendations.hpp"

namespace ff {

std::vector<Tip> recommend(const SessionContext&) { return {}; }

const char* to_string(TipSeverity s) noexcept {
    switch (s) {
    case TipSeverity::Info: return "info";
    case TipSeverity::Suggestion: return "suggestion";
    case TipSeverity::Warning: return "warning";
    }
    return "?";
}

} // namespace ff
