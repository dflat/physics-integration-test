#pragma once
#include <ecs/ecs.hpp>

class PhysicsSystem {
public:
    static void Register(ecs::World& world);
    static void Update(ecs::World& world, float dt);
};
