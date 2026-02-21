#include "builder.hpp"
#include "../components.hpp"
#include "../physics_context.hpp"
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <algorithm>
#include <raylib.h>

using namespace ecs;

void PlatformBuilderSystem::Update(World& world) {
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
            ecs::Vec3 size = { 4.0f, 0.5f, 4.0f };

            // Character radius = 0.4; platform half-height = 0.25.
            // Default spawn: platform top at feet, so center = feet - half_h.
            constexpr float k_char_radius    = 0.4f;
            constexpr float k_platform_half_h = 0.25f;
            float feet_y  = player_pos.y - k_char_radius;
            float spawn_y = feet_y - k_platform_half_h;

            // Cast a short ray downward from the feet to detect static geometry
            // within the platform volume. If we'd overlap it, snap on top.
            auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
            if (ctx_ptr && *ctx_ptr) {
                auto& ctx = **ctx_ptr;
                JPH::RRayCast ray{
                    JPH::RVec3(player_pos.x, feet_y, player_pos.z),
                    JPH::Vec3(0.f, -(size.y + 0.01f), 0.f) // down by full platform height + epsilon
                };
                JPH::RayCastResult result;
                JPH::DefaultBroadPhaseLayerFilter bp_filter(
                    ctx.object_vs_broadphase_layer_filter, Layers::NON_MOVING);
                JPH::DefaultObjectLayerFilter obj_filter(
                    ctx.object_layer_pair_filter, Layers::NON_MOVING);
                JPH::BodyFilter body_filter;

                if (ctx.physics_system->GetNarrowPhaseQuery().CastRay(
                        ray, result, bp_filter, obj_filter, body_filter)) {
                    // result.mFraction is in [0,1] along the ray direction
                    float surface_top  = feet_y + result.mFraction * ray.mDirection.GetY();
                    float surface_based = surface_top + k_platform_half_h;
                    spawn_y = std::max(spawn_y, surface_based);
                }
            }

            ecs::Vec3 spawn_pos = { player_pos.x, spawn_y, player_pos.z };

            world.deferred().create_with(
                ecs::LocalTransform{spawn_pos, {0,0,0,1}, size},
                ecs::WorldTransform{},
                MeshRenderer{ShapeType::Box, Colors::Maroon},
                BoxCollider{{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f}},
                RigidBodyConfig{BodyType::Static},
                WorldTag{}
            );
        }
    });
}
