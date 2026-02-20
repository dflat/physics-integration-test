#pragma once
#include <ecs/ecs.hpp>
#include <string>

// ---------------------------------------------------------------------------
// SceneLoader — reads JSON scene files and populates an ECS World.
//
// Components are added in lifecycle-safe order (colliders before rigid_body,
// transform before character) so on_add hooks fire with sibling data present.
// No Jolt or Raylib dependency — compilable in the headless test target.
// ---------------------------------------------------------------------------

class SceneLoader {
public:
    // Load entities from a JSON file into world.
    // Returns false if the file cannot be opened or the JSON is malformed.
    static bool load(ecs::World& world, const std::string& path);

    // Parse and spawn from a JSON string — identical to load() but avoids
    // file I/O. Intended for unit testing.
    static bool load_from_string(ecs::World& world, const std::string& json);

    // Destroy all WorldTag entities and flush deferred commands.
    static void unload(ecs::World& world);
};
