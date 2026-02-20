#include "character_motor.hpp"
#include "../components.hpp"
#include "../physics_handles.hpp"
#include "../physics_context.hpp"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <ecs/modules/transform.hpp>
#include <ecs/integration/glm.hpp>

using namespace ecs;

void CharacterMotorSystem::Register(World& world) {
    world.on_add<CharacterControllerConfig>(
        [&](World& w, Entity e, CharacterControllerConfig& cfg) {
            auto* ctx_ptr = w.try_resource<std::shared_ptr<PhysicsContext>>();
            if (!ctx_ptr || !*ctx_ptr) return;
            auto& ctx = **ctx_ptr;

            JPH::RefConst<JPH::ShapeSettings> shape_settings =
                new JPH::RotatedTranslatedShapeSettings(
                    JPH::Vec3(0, 0.5f * cfg.height, 0), JPH::Quat::sIdentity(),
                    new JPH::CapsuleShapeSettings(0.5f * cfg.height, cfg.radius));

            auto shape_result = shape_settings->Create();
            if (shape_result.HasError()) return;

            JPH::RVec3 pos = JPH::RVec3::sZero();
            if (auto* lt = w.try_get<LocalTransform>(e)) {
                pos = MathBridge::ToJolt(lt->position);
            }

            JPH::CharacterVirtualSettings settings;
            settings.mMass             = cfg.mass;
            settings.mMaxSlopeAngle    = JPH::DegreesToRadians(cfg.max_slope_angle);
            settings.mShape            = shape_result.Get();
            settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cfg.radius);

            auto character = std::make_shared<JPH::CharacterVirtual>(
                &settings, pos, JPH::Quat::sIdentity(), ctx.physics_system);

            w.add(e, CharacterHandle{character});
        });
}

void CharacterMotorSystem::Update(World& world, float dt) {
    auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
    if (!ctx_ptr || !*ctx_ptr) return;
    auto& ctx = **ctx_ptr;

    world.each<CharacterHandle, CharacterIntent, CharacterState, WorldTransform>(
        [&](Entity e, CharacterHandle& h, CharacterIntent& intent,
            CharacterState& state, WorldTransform& wt) {
            auto* ch = h.character.get();
            JPH::Vec3 current_vel = ch->GetLinearVelocity();

            // --- Horizontal Movement ---
            JPH::Vec3 move_dir   = MathBridge::ToJolt(intent.move_dir);
            bool      on_ground  = (state.mode == CharacterState::Mode::Grounded);
            float     accel      = on_ground ? 15.0f : 5.0f;

            JPH::Vec3 target_vel     = move_dir * 10.0f;
            JPH::Vec3 horizontal_vel = {current_vel.GetX(), 0.0f, current_vel.GetZ()};
            horizontal_vel += (target_vel - horizontal_vel) * accel * dt;

            // --- Vertical Movement ---
            float vertical_vel = current_vel.GetY();

            if (state.jump_impulse > 0.0f) {
                vertical_vel = state.jump_impulse;
            } else if (!on_ground) {
                float gravity = (vertical_vel < 0.0f) ? -40.0f : -25.0f;
                vertical_vel += gravity * dt;
            } else {
                vertical_vel = 0.0f;
            }

            JPH::Vec3 new_vel = horizontal_vel;
            new_vel.SetY(vertical_vel);
            ch->SetLinearVelocity(new_vel);

            // --- Rotation (face direction of travel) ---
            if (horizontal_vel.LengthSq() > 0.1f) {
                JPH::Vec3 look_dir   = horizontal_vel.Normalized();
                float     angle      = atan2f(look_dir.GetX(), look_dir.GetZ());
                JPH::Quat target_rot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), angle);
                ch->SetRotation(
                    ch->GetRotation().SLERP(target_rot, 10.0f * dt).Normalized());
            }

            // --- Extended Update (steps the character through the world) ---
            JPH::DefaultBroadPhaseLayerFilter bp_filter(
                ctx.object_vs_broadphase_layer_filter, Layers::MOVING);
            JPH::DefaultObjectLayerFilter obj_filter(
                ctx.object_layer_pair_filter, Layers::MOVING);
            JPH::BodyFilter  body_filter;
            JPH::ShapeFilter shape_filter;
            JPH::CharacterVirtual::ExtendedUpdateSettings ext_settings;

            ch->ExtendedUpdate(dt, {0, -9.81f, 0}, ext_settings,
                               bp_filter, obj_filter, body_filter, shape_filter,
                               *ctx.temp_allocator);

            // --- Sync Jolt position back to ECS transforms ---
            if (auto* lt = world.try_get<LocalTransform>(e)) {
                lt->position = MathBridge::FromJolt(ch->GetPosition());
                lt->rotation = MathBridge::FromJolt(ch->GetRotation());
                wt.matrix    = mat4_compose(lt->position, lt->rotation, lt->scale);
            }
        });
}
