// This file provides the main() function for Catch2 tests
// Used for consistency across all platforms

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    // This is the main entry point for all tests
    return Catch::Session().run(argc, argv);
}
