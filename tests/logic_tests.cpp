#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/math_util.hpp"

// CharacterStateSystem::apply_state tests are deferred to RFC-0007.
// components.hpp currently includes <raylib.h> and Jolt headers, which prevents
// headless compilation. RFC-0007 (Component Type Purity) removes those dependencies,
// at which point apply_state can be exercised from the test target without linking
// the full engine stack.

using namespace engine::math;

TEST_CASE("Angle Normalization", "[math]") {
    const float pi = 3.1415926535f;
    
    SECTION("Inside range") {
        CHECK_THAT(normalize_angle(1.0f), Catch::Matchers::WithinRel(1.0f));
        CHECK_THAT(normalize_angle(-1.0f), Catch::Matchers::WithinRel(-1.0f));
    }
    
    SECTION("Outside range (positive)") {
        CHECK_THAT(normalize_angle(1.5f * pi), Catch::Matchers::WithinRel(-0.5f * pi));
        CHECK_THAT(normalize_angle(3.0f * pi), Catch::Matchers::WithinRel(pi));
    }
    
    SECTION("Outside range (negative)") {
        CHECK_THAT(normalize_angle(-1.5f * pi), Catch::Matchers::WithinRel(0.5f * pi));
        CHECK_THAT(normalize_angle(-3.0f * pi), Catch::Matchers::WithinRel(-pi));
    }
}

TEST_CASE("Follow Angle Calculation", "[math]") {
    SECTION("Moving Forward (+Z)") {
        // atan2(-0, -1) can be PI or -PI depending on implementation.
        // We normalize both to ensure consistency.
        float result = normalize_angle(calculate_follow_angle(0, 1));
        CHECK_THAT(std::abs(result), Catch::Matchers::WithinRel(3.1415926535f));
    }
    
    SECTION("Moving Right (+X)") {
        float result = normalize_angle(calculate_follow_angle(1, 0));
        CHECK_THAT(result, Catch::Matchers::WithinRel(-1.5707963267f));
    }
}

TEST_CASE("Alignment (Dot Product) Calculation", "[math]") {
    SECTION("Perfect alignment") {
        CHECK(calculate_alignment(0, 1, 0, 1) == 1.0f);
    }

    SECTION("Opposite alignment") {
        CHECK(calculate_alignment(0, 1, 0, -1) == -1.0f);
    }

    SECTION("Perpendicular") {
        CHECK(calculate_alignment(1, 0, 0, 1) == 0.0f);
    }
}

