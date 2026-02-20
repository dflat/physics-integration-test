#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/math_util.hpp"
#include "../src/systems/character_state.hpp"

// components.hpp and character_state.hpp are now free of engine-library
// dependencies (RFC-0008), so apply_state can be exercised without linking
// Jolt or Raylib.

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

// ---------------------------------------------------------------------------
// CharacterStateSystem::apply_state
// ---------------------------------------------------------------------------

TEST_CASE("apply_state — jump_impulse is cleared each call", "[character_state]") {
    // jump_impulse is a one-frame signal; must be zeroed at the top of every call.
    CharacterIntent intent{};
    CharacterState  state{};
    state.jump_impulse = 99.0f; // stale value from a previous frame

    CharacterStateSystem::apply_state(true, 0.016f, intent, state);

    CHECK(state.jump_impulse == 0.0f);
}

TEST_CASE("apply_state — grounded resets air state", "[character_state]") {
    CharacterIntent intent{};
    CharacterState  state{};
    state.mode       = CharacterState::Mode::Airborne;
    state.jump_count = 1;
    state.air_time   = 0.8f;

    CharacterStateSystem::apply_state(true, 0.016f, intent, state);

    CHECK(state.mode       == CharacterState::Mode::Grounded);
    CHECK(state.jump_count == 0);
    CHECK(state.air_time   == 0.0f);
    CHECK(state.jump_impulse == 0.0f);
}

TEST_CASE("apply_state — airborne increments air_time", "[character_state]") {
    CharacterIntent intent{};
    CharacterState  state{};
    state.mode     = CharacterState::Mode::Grounded;
    state.air_time = 0.0f;

    CharacterStateSystem::apply_state(false, 0.016f, intent, state);

    CHECK(state.mode == CharacterState::Mode::Airborne);
    CHECK_THAT(state.air_time, Catch::Matchers::WithinAbs(0.016f, 1e-5f));
}

TEST_CASE("apply_state — first jump from ground", "[character_state]") {
    CharacterIntent intent{};
    intent.jump_requested = true;
    CharacterState state{};
    state.mode = CharacterState::Mode::Grounded;

    CharacterStateSystem::apply_state(true, 0.016f, intent, state);

    CHECK_THAT(state.jump_impulse, Catch::Matchers::WithinRel(12.0f));
    CHECK(state.jump_count == 1);
}

TEST_CASE("apply_state — double jump from air", "[character_state]") {
    CharacterIntent intent{};
    intent.jump_requested = true;
    CharacterState state{};
    state.mode       = CharacterState::Mode::Airborne;
    state.jump_count = 1;
    state.air_time   = 0.5f;

    CharacterStateSystem::apply_state(false, 0.016f, intent, state);

    CHECK_THAT(state.jump_impulse, Catch::Matchers::WithinRel(10.0f));
    CHECK(state.jump_count == 2);
}

TEST_CASE("apply_state — no jump when exhausted", "[character_state]") {
    CharacterIntent intent{};
    intent.jump_requested = true;
    CharacterState state{};
    state.mode       = CharacterState::Mode::Airborne;
    state.jump_count = 2; // both jumps used
    state.air_time   = 0.5f;

    CharacterStateSystem::apply_state(false, 0.016f, intent, state);

    CHECK(state.jump_impulse == 0.0f);
    CHECK(state.jump_count   == 2);
}

TEST_CASE("apply_state — coyote jump (airborne, first jump still available)", "[character_state]") {
    // Walked off a ledge: airborne but jump_count==0, within coyote window.
    CharacterIntent intent{};
    intent.jump_requested = true;
    CharacterState state{};
    state.mode       = CharacterState::Mode::Airborne;
    state.jump_count = 0;
    state.air_time   = 0.1f; // within coyote window (< 0.2s)

    CharacterStateSystem::apply_state(false, 0.016f, intent, state);

    // Coyote path: consumes the first jump and increments → both jumps spent.
    CHECK_THAT(state.jump_impulse, Catch::Matchers::WithinRel(12.0f));
    CHECK(state.jump_count == 2);
}

TEST_CASE("apply_state — no jump without input", "[character_state]") {
    CharacterIntent intent{}; // jump_requested == false
    CharacterState  state{};
    state.mode = CharacterState::Mode::Grounded;

    CharacterStateSystem::apply_state(true, 0.016f, intent, state);

    CHECK(state.jump_impulse == 0.0f);
    CHECK(state.jump_count   == 0);
}
