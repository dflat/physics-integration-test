#pragma once
#include "../components.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>

using namespace ecs;

class PlatformBuilderSystem {
public:
    static void Update(World& world) {
        world.each<PlayerTag, WorldTransform, PlayerInput>([&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input) {
            if (input.plant_platform) {
                // Throttle: only one platform per trigger press? 
                // Actually, our System_Input resets it every frame, 
                // but gamepad trigger might stay > 0.5 for many frames.
                // Let's use a static timer to prevent "machine gun" platforms.
                static float cooldown = 0.0f;
                cooldown -= GetFrameTime();
                if (cooldown > 0) return;
                cooldown = 0.3f;

                ecs::Vec3 player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                ecs::Vec3 spawn_pos = { player_pos.x, player_pos.y - 0.2f, player_pos.z };
                ecs::Vec3 size = { 4.0f, 0.5f, 4.0f };

                world.deferred().create_with(
                    ecs::LocalTransform{spawn_pos, {0,0,0,1}, size},
                    ecs::WorldTransform{},
                    MeshRenderer{0, MAROON},
                    BoxCollider{{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f}},
                    RigidBodyConfig{BodyType::Static}
                );
            }
        });
    }
};
