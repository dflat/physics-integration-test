#pragma once
#include "../components.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>

using namespace ecs;

class PlatformBuilderSystem {
public:
    static void Update(World& world) {
        float dt = GetFrameTime();
        world.each<PlayerTag, WorldTransform, PlayerInput, PlayerState>([&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, PlayerState& state) {
            // Update cooldown
            if (state.build_cooldown > 0) {
                state.build_cooldown -= dt;
            }

            // Detect rising edge of trigger/key
            bool trigger_is_down = input.plant_platform || input.trigger_val > 0.5f;
            bool trigger_pressed = trigger_is_down && !state.trigger_was_down;
            state.trigger_was_down = trigger_is_down;

            if (trigger_pressed && state.build_cooldown <= 0) {
                state.build_cooldown = 0.25f; // Responsive cooldown

                // Position: beneath player
                ecs::Vec3 player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                ecs::Vec3 spawn_pos = { player_pos.x, player_pos.y - 0.2f, player_pos.z };
                ecs::Vec3 size = { 4.0f, 0.5f, 4.0f };

                world.deferred().create_with(
                    ecs::LocalTransform{spawn_pos, {0,0,0,1}, size},
                    ecs::WorldTransform{},
                    MeshRenderer{0, MAROON},
                    BoxCollider{{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f}},
                    RigidBodyConfig{BodyType::Static},
                    WorldTag{}
                );
            }
        });
    }
};
