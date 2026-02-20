#include "physics.hpp"
#include "../components.hpp"
#include "../physics_handles.hpp"
#include "../physics_context.hpp"
#include <ecs/modules/transform.hpp>
#include <ecs/integration/glm.hpp>
#include <memory>

using namespace ecs;

void PhysicsSystem::Register(World& world) {
    world.on_add<RigidBodyConfig>([&](World& w, Entity e, RigidBodyConfig& cfg) {
        if (w.has<RigidBodyHandle>(e)) return;

        auto* ctx_ptr = w.try_resource<std::shared_ptr<PhysicsContext>>();
        if (!ctx_ptr || !*ctx_ptr) return;
        auto& ctx = **ctx_ptr;

        JPH::BodyInterface& bi = ctx.GetBodyInterface();

        JPH::RefConst<JPH::Shape> shape;
        if (auto* box = w.try_get<BoxCollider>(e)) {
            shape = new JPH::BoxShape(MathBridge::ToJolt(box->half_extents));
        } else if (auto* sphere = w.try_get<SphereCollider>(e)) {
            shape = new JPH::SphereShape(sphere->radius);
        } else {
            shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
        }

        JPH::Vec3 pos = JPH::Vec3::sZero();
        JPH::Quat rot = JPH::Quat::sIdentity();

        if (auto* lt = w.try_get<LocalTransform>(e)) {
            pos = MathBridge::ToJolt(lt->position);
            rot = MathBridge::ToJolt(lt->rotation);
        } else if (auto* wt = w.try_get<WorldTransform>(e)) {
             ecs::Vec3 p = { wt->matrix.m[12], wt->matrix.m[13], wt->matrix.m[14] };
             pos = MathBridge::ToJolt(p);
        }

        JPH::EMotionType motion = JPH::EMotionType::Dynamic;
        if (cfg.type == BodyType::Static) motion = JPH::EMotionType::Static;
        if (cfg.type == BodyType::Kinematic) motion = JPH::EMotionType::Kinematic;

        JPH::ObjectLayer layer = (cfg.type == BodyType::Static) ? Layers::NON_MOVING : Layers::MOVING;

        JPH::BodyCreationSettings settings(shape, pos, rot, motion, layer);
        settings.mRestitution = cfg.restitution;
        settings.mFriction = cfg.friction;
        settings.mIsSensor = cfg.sensor;
        
        JPH::Body* body = bi.CreateBody(settings);
        bi.AddBody(body->GetID(), JPH::EActivation::Activate);

        w.add(e, RigidBodyHandle{body->GetID()});
    });

    world.on_remove<RigidBodyHandle>([&](World& w, Entity, RigidBodyHandle& h) {
        auto* ctx_ptr = w.try_resource<std::shared_ptr<PhysicsContext>>();
        if (!ctx_ptr || !*ctx_ptr) return;
        JPH::BodyInterface& bi = (*ctx_ptr)->GetBodyInterface();
        bi.RemoveBody(h.id);
        bi.DestroyBody(h.id);
    });
}

void PhysicsSystem::Update(World& world, float dt) {
    auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
    if (!ctx_ptr || !*ctx_ptr) return;
    auto& ctx = **ctx_ptr;

    ctx.physics_system->Update(dt, 1, ctx.temp_allocator, ctx.job_system);

    JPH::BodyInterface& bi = ctx.GetBodyInterface();
    
    world.each<RigidBodyHandle, WorldTransform, RigidBodyConfig>(
        [&](Entity e, RigidBodyHandle& h, WorldTransform& wt, RigidBodyConfig& cfg) {
            if (cfg.type == BodyType::Dynamic) {
                JPH::RVec3 pos;
                JPH::Quat rot;
                bi.GetPositionAndRotation(h.id, pos, rot);

                wt.matrix = mat4_compose(MathBridge::FromJolt(pos), MathBridge::FromJolt(rot), {1,1,1});
                
                if (auto* lt = world.try_get<LocalTransform>(e)) {
                     lt->position = MathBridge::FromJolt(pos);
                     lt->rotation = MathBridge::FromJolt(rot);
                }
            }
        });
}
