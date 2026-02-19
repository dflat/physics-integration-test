#pragma once
#include <ecs/ecs.hpp>

// Translates PlayerInput + camera view directions into a world-space CharacterIntent.
// Runs in the Logic phase, after CameraSystem has written view_forward/view_right.
class CharacterInputSystem {
public:
    static void Register(ecs::World& world);
    static void Update(ecs::World& world, float dt);
};
