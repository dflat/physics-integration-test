#pragma once
#include <ecs/ecs.hpp>

// Applies CharacterIntent + CharacterState to Jolt: velocity, rotation,
// ExtendedUpdate, and transform sync back to ECS.
// Must run last in the Logic phase, immediately before PhysicsSystem.
class CharacterMotorSystem {
public:
    static void Register(ecs::World& world);
    static void Update(ecs::World& world, float dt);
};
