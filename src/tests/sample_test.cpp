#include <catch2/catch_test_macros.hpp>

TEST_CASE("Sample test to verify Catch2 is working", "[sample]") {
    REQUIRE(1 + 1 == 2);

    SECTION("Basic arithmetic") {
        REQUIRE(2 * 2 == 4);
        REQUIRE(10 / 2 == 5);
    }

    SECTION("String comparison") {
        std::string str = "Hello, Catch2!";
        REQUIRE(str.length() == 14);
        REQUIRE(str.find("Catch2") != std::string::npos);
    }
}

TEST_CASE("Vector operations", "[containers]") {
    std::vector<int> vec = {1, 2, 3, 4, 5};

    REQUIRE(vec.size() == 5);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[4] == 5);

    vec.push_back(6);
    REQUIRE(vec.size() == 6);
    REQUIRE(vec.back() == 6);
}
