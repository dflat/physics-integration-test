#pragma once
#include <ecs/ecs.hpp>
#include <ecs/integration/glm.hpp>
#include "../components.hpp"
#include "../physics_context.hpp"
#include <iostream>

using namespace ecs;

class PhysicsSystem {
public:
    static void Register(World& world) {
        // --- 1. Body Creation (Config -> Handle) ---
        world.on_add<RigidBodyConfig>([&](World& w, Entity e, RigidBodyConfig& cfg) {
            // Avoid creating if already has handle (e.g. from prefab clone)
            if (w.has<RigidBodyHandle>(e)) return;

            auto* ctx_ptr = w.try_resource<std::shared_ptr<PhysicsContext>>();
            if (!ctx_ptr || !*ctx_ptr) return;
            auto& ctx = **ctx_ptr;

            JPH::BodyInterface& bi = ctx.GetBodyInterface();

            // 1. Determine Shape
            JPH::RefConst<JPH::Shape> shape;
            if (auto* box = w.try_get<BoxCollider>(e)) {
                shape = new JPH::BoxShape(MathBridge::ToJolt(box->half_extents));
            } else if (auto* sphere = w.try_get<SphereCollider>(e)) {
                shape = new JPH::SphereShape(sphere->radius);
            } else {
                // Default fallback
                shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
            }

            // 2. Initial Transform
            JPH::Vec3 pos = JPH::Vec3::sZero();
            JPH::Quat rot = JPH::Quat::sIdentity();

            // Prefer LocalTransform for initial spawn parameters as WorldTransform might be stale (Identity)
            if (auto* lt = w.try_get<LocalTransform>(e)) {
                pos = MathBridge::ToJolt(lt->position);
                rot = MathBridge::ToJolt(lt->rotation);
            } else if (auto* wt = w.try_get<WorldTransform>(e)) {
                 ecs::Vec3 p = { wt->matrix.m[12], wt->matrix.m[13], wt->matrix.m[14] };
                 pos = MathBridge::ToJolt(p);
                 // Rotation extraction from matrix is complex, assume identity if no LocalTransform
            }

            // 3. Settings
            JPH::EMotionType motion = JPH::EMotionType::Dynamic;
            if (cfg.type == BodyType::Static) motion = JPH::EMotionType::Static;
            if (cfg.type == BodyType::Kinematic) motion = JPH::EMotionType::Kinematic;

            JPH::ObjectLayer layer = (cfg.type == BodyType::Static) ? Layers::NON_MOVING : Layers::MOVING;

            JPH::BodyCreationSettings settings(shape, pos, rot, motion, layer);
            settings.mRestitution = cfg.restitution;
            settings.mFriction = cfg.friction;
            settings.mIsSensor = cfg.sensor;
            
            // 4. Create Body
            JPH::Body* body = bi.CreateBody(settings);
            bi.AddBody(body->GetID(), JPH::EActivation::Activate);

            // 5. Store Handle
            w.add(e, RigidBodyHandle{body->GetID()});
        });

        // --- 2. Body Destruction ---
        world.on_remove<RigidBodyHandle>([&](World& w, Entity, RigidBodyHandle& h) {
            auto* ctx_ptr = w.try_resource<std::shared_ptr<PhysicsContext>>();
            if (!ctx_ptr || !*ctx_ptr) return;
            JPH::BodyInterface& bi = (*ctx_ptr)->GetBodyInterface();
            bi.RemoveBody(h.id);
            bi.DestroyBody(h.id);
        });
    }

    static void Update(World& world, float dt) {
        auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
        if (!ctx_ptr || !*ctx_ptr) return;
        auto& ctx = **ctx_ptr;

        // 1. Step Physics
        // We use 1 collision step per frame for simplicity
        ctx.physics_system->Update(dt, 1, ctx.temp_allocator, ctx.job_system);

        // 2. Sync: Jolt -> ECS
        JPH::BodyInterface& bi = ctx.GetBodyInterface();
        
        // We only sync Dynamic bodies back to ECS
        // Static/Kinematic are usually driven BY the ECS
        world.each<RigidBodyHandle, WorldTransform, RigidBodyConfig>(
            [&](Entity e, RigidBodyHandle& h, WorldTransform& wt, RigidBodyConfig& cfg) {
                if (cfg.type == BodyType::Dynamic) {
                    JPH::RVec3 pos;
                    JPH::Quat rot;
                    bi.GetPositionAndRotation(h.id, pos, rot);

                    // Recompose WorldTransform
                    // We assume scale is 1.0 or preserved from LocalTransform. 
                    // ideally we'd read scale from LocalTransform.
                    // For this prototype, we just set Matrix directly from PRS.
                    wt.matrix = mat4_compose(MathBridge::FromJolt(pos), MathBridge::FromJolt(rot), {1,1,1});
                    
                    // Also update LocalTransform if it exists, so the rest of the game logic sees it
                    if (auto* lt = world.try_get<LocalTransform>(e)) {
                         // Note: This logic assumes no parent. If parent exists, we need inverse parent transform.
                         // For this prototype, we assume physics objects are root.
                         lt->position = MathBridge::FromJolt(pos);
                         lt->rotation = MathBridge::FromJolt(rot);
                    }
                }
            });
    }
};
