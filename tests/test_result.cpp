#include <doctest/doctest.h>

#include <string>

#include "freeframe/result.hpp"

TEST_CASE("Result success") {
    auto r = ff::Result<int>::success(42);
    CHECK(r.ok());
    CHECK(r.value() == 42);
    CHECK(r.error().empty());
}

TEST_CASE("Result failure") {
    auto r = ff::Result<int>::failure("boom");
    CHECK_FALSE(r.ok());
    CHECK(r.error() == "boom");
}

TEST_CASE("Result with string payload") {
    auto ok = ff::Result<std::string>::success("x");
    auto bad = ff::Result<std::string>::failure("y");
    CHECK(ok.ok());
    CHECK(ok.value() == "x");
    CHECK_FALSE(bad.ok());
    CHECK(bad.error() == "y");
}
