#pragma once

#include <cassert>
#include <string>
#include <utility>
#include <variant>

namespace ff {

// Minimal value-or-error type (std::expected is not available everywhere in C++20).
template <typename T>
class Result {
public:
    static Result success(T value) {
        Result r;
        r.m_data.template emplace<1>(std::move(value));
        return r;
    }
    static Result failure(std::string message) {
        Result r;
        r.m_data.template emplace<2>(std::move(message));
        return r;
    }

    bool ok() const noexcept { return m_data.index() == 1; }
    explicit operator bool() const noexcept { return ok(); }

    T& value() & {
        assert(ok());
        return std::get<1>(m_data);
    }
    const T& value() const& {
        assert(ok());
        return std::get<1>(m_data);
    }
    T&& value() && {
        assert(ok());
        return std::get<1>(std::move(m_data));
    }

    // Empty string when ok().
    const std::string& error() const noexcept {
        static const std::string kEmpty;
        return m_data.index() == 2 ? std::get<2>(m_data) : kEmpty;
    }

private:
    Result() = default;
    std::variant<std::monostate, T, std::string> m_data;
};

} // namespace ff
