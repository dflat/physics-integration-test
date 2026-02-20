#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <ecs/ecs.hpp>
#include <memory>

// ---------------------------------------------------------------------------
// Math Bridge  (Jolt <-> ECS conversions; lives here so that components.hpp
// remains free of Jolt headers and can be included in headless test targets)
// ---------------------------------------------------------------------------
namespace MathBridge {
    inline JPH::Vec3 ToJolt(const ecs::Vec3& v) { return {v.x, v.y, v.z}; }
    inline JPH::Quat ToJolt(const ecs::Quat& q) { return {q.x, q.y, q.z, q.w}; }

    inline ecs::Vec3 FromJolt(const JPH::Vec3& v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
    inline ecs::Quat FromJolt(const JPH::Quat& q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }
#ifdef JPH_DOUBLE_PRECISION
    inline ecs::Vec3 FromJolt(const JPH::RVec3& v) {
        return {static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())};
    }
#endif
}

// ---------------------------------------------------------------------------
// Physics Runtime Handles (Managed by PhysicsSystem / CharacterMotorSystem)
// Not data components — opaque links into the Jolt simulation.
// ---------------------------------------------------------------------------

struct RigidBodyHandle {
    JPH::BodyID id;
};

struct CharacterHandle {
    std::shared_ptr<JPH::CharacterVirtual> character;
};
