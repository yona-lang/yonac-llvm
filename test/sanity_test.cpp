#include <doctest/doctest.h>

// Simple test to verify doctest is working
TEST_CASE("Basic sanity test") {
    CHECK(1 + 1 == 2);
    CHECK(true);
}

TEST_CASE("String comparison test") {
    std::string hello = "hello";
    CHECK(hello == "hello");
}
