#pragma once
#include <ecs/ecs.hpp>

class CameraSystem {
public:
    static void Update(ecs::World& world, float dt);
};
