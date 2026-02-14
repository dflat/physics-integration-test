#pragma once
#include "../components.hpp"
#include "../physics_context.hpp"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <ecs/ecs.hpp>
#include <ecs/integration/glm.hpp>

using namespace ecs;

class CharacterSystem {
public:
  static void Register(World &world) {
    world.on_add<CharacterControllerConfig>(
        [&](World &w, Entity e, CharacterControllerConfig &cfg) {
          auto *ctx_ptr = w.try_resource<std::shared_ptr<PhysicsContext>>();
          if (!ctx_ptr || !*ctx_ptr) return;
          auto &ctx = **ctx_ptr;

          JPH::RefConst<JPH::ShapeSettings> shape_settings =
              new JPH::RotatedTranslatedShapeSettings(
                  JPH::Vec3(0, 0.5f * cfg.height, 0), JPH::Quat::sIdentity(),
                  new JPH::CapsuleShapeSettings(0.5f * cfg.height, cfg.radius));

          auto shape_result = shape_settings->Create();
          if (shape_result.HasError()) return;
          JPH::RefConst<JPH::Shape> shape = shape_result.Get();

          JPH::RVec3 pos = JPH::RVec3::sZero();
          if (auto *lt = w.try_get<LocalTransform>(e)) {
            pos = MathBridge::ToJolt(lt->position);
          }

          JPH::CharacterVirtualSettings settings;
          settings.mMass = cfg.mass;
          settings.mMaxSlopeAngle = JPH::DegreesToRadians(cfg.max_slope_angle);
          settings.mShape = shape;
          settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cfg.radius);

          auto character = std::make_shared<JPH::CharacterVirtual>(
              &settings, pos, JPH::Quat::sIdentity(), ctx.physics_system);

          w.add(e, CharacterHandle{character});
        });
  }

  static void Update(World &world, float dt) {
    auto *ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
    if (!ctx_ptr || !*ctx_ptr) return;
    auto &ctx = **ctx_ptr;

    world.each<CharacterHandle, PlayerInput, PlayerState, WorldTransform>(
        [&](Entity e, CharacterHandle &h, PlayerInput &input, PlayerState &state, WorldTransform &wt) {
          auto *ch = h.character.get();
          JPH::Vec3 current_vel = ch->GetLinearVelocity();
          
          // --- Ground State Detection ---
          auto ground_state = ch->GetGroundState();
          bool on_ground = ground_state == JPH::CharacterVirtual::EGroundState::OnGround;
          bool on_slope = ground_state == JPH::CharacterVirtual::EGroundState::OnSteepGround;
          
          if (on_ground) {
              state.jump_count = 0;
              state.air_time = 0.0f;
          } else {
              state.air_time += dt;
          }

          // --- 1. Dynamic Horizontal Movement (Acceleration Curve) ---
          JPH::Vec3 fwd = MathBridge::ToJolt(input.view_forward);
          JPH::Vec3 right = MathBridge::ToJolt(input.view_right);
          
          // Project forward and right onto the horizontal plane (X-Z)
          fwd.SetY(0); 
          right.SetY(0);
          
          if (fwd.LengthSq() > 0.001f) {
              fwd = fwd.Normalized();
          } else {
              // Fallback if looking straight up/down: use the 'right' vector to derive a forward
              // or just use the camera's forward without Y
              fwd = JPH::Vec3(input.view_forward.x, 0, input.view_forward.z);
              if (fwd.LengthSq() > 0.001f) fwd = fwd.Normalized();
              else fwd = JPH::Vec3::sAxisZ(); // Absolute fallback
          }
          
          if (right.LengthSq() > 0.001f) {
              right = right.Normalized();
          } else {
              right = fwd.Cross(JPH::Vec3::sAxisY()).Normalized();
          }

          JPH::Vec3 move_dir = (fwd * input.move_input.y + right * input.move_input.x);
          
          // Movement parameters
          float max_speed = 10.0f;
          // Smoothly interpolate between ground and air acceleration
          float accel_factor = on_ground ? 15.0f : 5.0f; 
          
          JPH::Vec3 target_vel = move_dir * max_speed;
          JPH::Vec3 horizontal_vel(current_vel.GetX(), 0, current_vel.GetZ());
          
          // Non-linear acceleration
          JPH::Vec3 vel_diff = target_vel - horizontal_vel;
          horizontal_vel += vel_diff * accel_factor * dt;

          // --- 2. Vertical Movement (Gravity & Double Jump) ---
          float vertical_vel = current_vel.GetY();
          
          // Gravity curve: stronger gravity when falling
          float gravity = (vertical_vel < 0) ? -40.0f : -25.0f; 
          
          // Coyote time: Allow a short window to jump after leaving ground
          bool can_coyote = (state.jump_count == 0 && state.air_time < 0.2f);
          bool can_jump = on_ground || can_coyote || state.jump_count < 2;

          if (input.jump && can_jump) {
              vertical_vel = (state.jump_count == 0) ? 12.0f : 10.0f; // Slightly weaker second jump

              // If we are in the air and haven't jumped yet, we use up the first jump slot (coyote)
              if (!on_ground && state.jump_count == 0) {
                  state.jump_count = 1;
              }
              
              state.jump_count++;
          } else if (!on_ground) {
              vertical_vel += gravity * dt;
          } else {
              // On ground, reset vertical velocity if not jumping
              // (Gravity force for moving bodies is passed to ExtendedUpdate)
              vertical_vel = 0.0f;
          }

          // Combine
          JPH::Vec3 new_vel = horizontal_vel;
          new_vel.SetY(vertical_vel);
          ch->SetLinearVelocity(new_vel);

          // --- 3. Rotation ---
          // Rotate character to face movement direction smoothly
          if (horizontal_vel.LengthSq() > 0.1f) {
              JPH::Vec3 look_dir = horizontal_vel.Normalized();
              // In Jolt, CharacterVirtual is usually +Z forward or we define it by its rotation
              // We'll calculate a rotation that faces the movement direction
              float angle = atan2f(look_dir.GetX(), look_dir.GetZ());
              JPH::Quat target_rot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), angle);
              
              // Smoothly interpolate rotation
              JPH::Quat current_rot = ch->GetRotation();
              ch->SetRotation(current_rot.SLERP(target_rot, 10.0f * dt).Normalized());
          }

          // --- 4. Step (Extended Update) ---
          JPH::DefaultBroadPhaseLayerFilter bp_filter(ctx.object_vs_broadphase_layer_filter, Layers::MOVING);
          JPH::DefaultObjectLayerFilter obj_filter(ctx.object_layer_pair_filter, Layers::MOVING);
          JPH::BodyFilter body_filter;
          JPH::ShapeFilter shape_filter;

          // Settings for StickToFloor and WalkStairs
          JPH::CharacterVirtual::ExtendedUpdateSettings settings;
          // Use a default gravity for downward force on other bodies
          JPH::Vec3 gravity_vec(0, -9.81f, 0); 

          ch->ExtendedUpdate(dt, gravity_vec, settings, bp_filter, obj_filter, body_filter, shape_filter, *ctx.temp_allocator);

          // --- 4. Sync ---
          JPH::RVec3 next_pos = ch->GetPosition();
          if (auto *lt = world.try_get<LocalTransform>(e)) {
            lt->position = MathBridge::FromJolt(next_pos);
            lt->rotation = MathBridge::FromJolt(ch->GetRotation());
            wt.matrix = mat4_compose(lt->position, lt->rotation, lt->scale);
          }
        });
  }
};
