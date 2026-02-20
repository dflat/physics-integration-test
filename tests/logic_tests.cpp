#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/math_util.hpp"
#include "../src/systems/character_state.hpp"
#include "../src/events.hpp"
#include "../src/scene.hpp"
#include "../src/debug_panel.hpp"
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>

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

// ---------------------------------------------------------------------------
// Events<T>
// ---------------------------------------------------------------------------

struct TestEvent { int value; };

TEST_CASE("Events — send and read", "[events]") {
    Events<TestEvent> queue;

    CHECK(queue.empty());
    CHECK(queue.read().empty());

    queue.send({42});
    queue.send({7});

    CHECK_FALSE(queue.empty());
    REQUIRE(queue.read().size() == 2);
    CHECK(queue.read()[0].value == 42);
    CHECK(queue.read()[1].value == 7);
}

TEST_CASE("Events — clear empties the queue", "[events]") {
    Events<TestEvent> queue;
    queue.send({1});
    queue.send({2});
    queue.clear();

    CHECK(queue.empty());
    CHECK(queue.read().empty());
}

TEST_CASE("Events — clear on empty queue is safe", "[events]") {
    Events<TestEvent> queue;
    queue.clear(); // must not crash or assert

    CHECK(queue.empty());
}

TEST_CASE("Events — multiple sends accumulate in order", "[events]") {
    Events<TestEvent> queue;
    for (int i = 0; i < 5; ++i) queue.send({i});

    const auto& v = queue.read();
    REQUIRE(v.size() == 5);
    for (int i = 0; i < 5; ++i) CHECK(v[i].value == i);
}

// ---------------------------------------------------------------------------
// SceneLoader
// ---------------------------------------------------------------------------

static const char* MINIMAL_SCENE = R"({
  "entities": [
    {
      "transform": { "position": [1.0, 2.0, 3.0], "rotation": [0,0,0,1], "scale": [4.0, 5.0, 6.0] },
      "mesh": { "shape": "Box", "color": [0.5, 0.5, 0.5, 1.0] },
      "box_collider": { "half_extents": [2.0, 2.5, 3.0] },
      "rigid_body": { "type": "Static" },
      "tags": ["World"]
    },
    {
      "transform": { "position": [0.0, 5.0, 0.0], "rotation": [0,0,0,1], "scale": [1,1,1] },
      "mesh": { "shape": "Capsule", "color": [1.0, 0.0, 0.0, 1.0] },
      "character": { "height": 2.0, "radius": 0.5, "mass": 80.0, "max_slope_angle": 50.0 },
      "tags": ["Player", "World"]
    }
  ]
})";

TEST_CASE("SceneLoader — correct entity count", "[scene]") {
    ecs::World world;
    REQUIRE(SceneLoader::load_from_string(world, MINIMAL_SCENE));
    CHECK(world.count() == 2);
}

TEST_CASE("SceneLoader — static entity has correct transform", "[scene]") {
    ecs::World world;
    SceneLoader::load_from_string(world, MINIMAL_SCENE);

    bool found = false;
    world.each<ecs::LocalTransform, BoxCollider>([&](ecs::Entity, ecs::LocalTransform& lt, BoxCollider&) {
        CHECK_THAT(lt.position.x, Catch::Matchers::WithinAbs(1.0f, 1e-4f));
        CHECK_THAT(lt.position.y, Catch::Matchers::WithinAbs(2.0f, 1e-4f));
        CHECK_THAT(lt.position.z, Catch::Matchers::WithinAbs(3.0f, 1e-4f));
        CHECK_THAT(lt.scale.x,    Catch::Matchers::WithinAbs(4.0f, 1e-4f));
        found = true;
    });
    CHECK(found);
}

TEST_CASE("SceneLoader — character entity has correct config", "[scene]") {
    ecs::World world;
    SceneLoader::load_from_string(world, MINIMAL_SCENE);

    bool found = false;
    world.each<CharacterControllerConfig, PlayerTag>([&](ecs::Entity,
                                                         CharacterControllerConfig& cfg,
                                                         PlayerTag&) {
        CHECK_THAT(cfg.height,          Catch::Matchers::WithinAbs(2.0f,  1e-4f));
        CHECK_THAT(cfg.radius,          Catch::Matchers::WithinAbs(0.5f,  1e-4f));
        CHECK_THAT(cfg.max_slope_angle, Catch::Matchers::WithinAbs(50.0f, 1e-4f));
        found = true;
    });
    CHECK(found);
}

TEST_CASE("SceneLoader — malformed JSON returns false", "[scene]") {
    ecs::World world;
    CHECK_FALSE(SceneLoader::load_from_string(world, "{bad json"));
    CHECK(world.count() == 0);
}

TEST_CASE("SceneLoader — player entity gets PlayerInput and PlayerState", "[scene]") {
    ecs::World world;
    SceneLoader::load_from_string(world, MINIMAL_SCENE);

    int player_count = 0;
    world.each<PlayerTag, PlayerInput, PlayerState>([&](ecs::Entity, PlayerTag&, PlayerInput&, PlayerState&) {
        ++player_count;
    });
    CHECK(player_count == 1);
}

// ---------------------------------------------------------------------------
// DebugPanel
// ---------------------------------------------------------------------------

TEST_CASE("DebugPanel — watch creates section and row", "[debug]") {
    DebugPanel panel;
    panel.watch("Engine", "FPS", []() { return std::string("60"); });

    REQUIRE(panel.sections().size() == 1);
    CHECK(panel.sections()[0].title == "Engine");
    REQUIRE(panel.sections()[0].rows.size() == 1);
    CHECK(panel.sections()[0].rows[0].label == "FPS");
    CHECK(panel.sections()[0].rows[0].fn() == "60");
}

TEST_CASE("DebugPanel — multiple rows in one section", "[debug]") {
    DebugPanel panel;
    panel.watch("Engine", "FPS",        []() { return std::string("60"); });
    panel.watch("Engine", "Frame Time", []() { return std::string("16 ms"); });
    panel.watch("Engine", "Entities",   []() { return std::string("15"); });

    REQUIRE(panel.sections().size() == 1);
    CHECK(panel.sections()[0].rows.size() == 3);
    CHECK(panel.sections()[0].rows[1].label == "Frame Time");
    CHECK(panel.sections()[0].rows[2].fn() == "15");
}

TEST_CASE("DebugPanel — multiple sections ordered by insertion", "[debug]") {
    DebugPanel panel;
    panel.watch("Engine",    "FPS",  []() { return std::string("60"); });
    panel.watch("Character", "Mode", []() { return std::string("Grounded"); });

    REQUIRE(panel.sections().size() == 2);
    CHECK(panel.sections()[0].title == "Engine");
    CHECK(panel.sections()[1].title == "Character");
}

TEST_CASE("DebugPanel — provider is called and returns current value", "[debug]") {
    int counter = 0;
    DebugPanel panel;
    panel.watch("Test", "Count", [&counter]() { return std::to_string(counter); });

    CHECK(panel.sections()[0].rows[0].fn() == "0");
    counter = 42;
    CHECK(panel.sections()[0].rows[0].fn() == "42");
}

TEST_CASE("DebugPanel — visible defaults to false, toggle works", "[debug]") {
    DebugPanel panel;
    CHECK_FALSE(panel.visible);
    panel.visible = true;
    CHECK(panel.visible);
}
