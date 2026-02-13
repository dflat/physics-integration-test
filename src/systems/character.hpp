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

    world.each<CharacterHandle, PlayerInput, WorldTransform>(
        [&](Entity e, CharacterHandle &h, PlayerInput &input, WorldTransform &wt) {
          auto *ch = h.character.get();
          JPH::Vec3 current_vel = ch->GetLinearVelocity();
          
          // --- 1. Horizontal Movement ---
          JPH::Vec3 fwd = MathBridge::ToJolt(input.view_forward);
          JPH::Vec3 right = MathBridge::ToJolt(input.view_right);
          fwd.SetY(0); right.SetY(0);
          if (fwd.LengthSq() > 0.001f) fwd = fwd.Normalized();
          if (right.LengthSq() > 0.001f) right = right.Normalized();

          JPH::Vec3 move_dir = (fwd * input.move_input.y + right * input.move_input.x);
          
          float max_speed = 9.0f;
          float acceleration = 70.0f;
          float friction = 18.0f;

          JPH::Vec3 horizontal_vel(current_vel.GetX(), 0, current_vel.GetZ());
          
          if (move_dir.LengthSq() > 0.001f) {
              horizontal_vel += move_dir * acceleration * dt;
              if (horizontal_vel.Length() > max_speed) {
                  horizontal_vel = horizontal_vel.Normalized() * max_speed;
              }
          } else {
              float speed = horizontal_vel.Length();
              if (speed > 0) {
                  float drop = speed * friction * dt;
                  float new_speed = std::max(0.0f, speed - drop);
                  horizontal_vel *= (new_speed / speed);
              }
          }

          // --- 2. Dynamic Vertical Movement ---
          auto ground_state = ch->GetGroundState();
          bool on_ground = ground_state == JPH::CharacterVirtual::EGroundState::OnGround;
          
          float vertical_vel = current_vel.GetY();

          // Variable Gravity: fall faster than you rise
          float gravity = (vertical_vel < 0) ? -35.0f : -20.0f; 
          
          if (on_ground && vertical_vel <= 0.01f) {
              vertical_vel = -0.1f; // Small downward force to stay grounded
              if (input.jump) {
                  vertical_vel = 11.0f; // Snappy jump impulse
              }
          } else {
              vertical_vel += gravity * dt;
          }

          // Combine
          JPH::Vec3 new_vel = horizontal_vel;
          new_vel.SetY(vertical_vel);
          ch->SetLinearVelocity(new_vel);

          // --- 3. Step ---
          JPH::DefaultBroadPhaseLayerFilter bp_filter(ctx.object_vs_broadphase_layer_filter, Layers::MOVING);
          JPH::DefaultObjectLayerFilter obj_filter(ctx.object_layer_pair_filter, Layers::MOVING);
          JPH::BodyFilter body_filter;
          JPH::ShapeFilter shape_filter;

          ch->Update(dt, JPH::Vec3::sZero(), bp_filter, obj_filter, body_filter, shape_filter, *ctx.temp_allocator);

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
