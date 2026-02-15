#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>
#include <memory>
#include <raylib.h>

// ---------------------------------------------------------------------------
// Math Bridge
// ---------------------------------------------------------------------------
namespace MathBridge {
    inline JPH::Vec3 ToJolt(const ecs::Vec3& v) { return {v.x, v.y, v.z}; }
    inline JPH::Quat ToJolt(const ecs::Quat& q) { return {q.x, q.y, q.z, q.w}; }
    
    inline ecs::Vec3 FromJolt(const JPH::Vec3& v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
    inline ecs::Quat FromJolt(const JPH::Quat& q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }
    // RVec3 is typedef to Vec3 in single precision, causing redefinition. Only define if Double Precision.
#ifdef JPH_DOUBLE_PRECISION
    inline ecs::Vec3 FromJolt(const JPH::RVec3& v) { return {static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())}; }
#endif

    inline Vector3 ToRaylib(const ecs::Vec3& v) { return {v.x, v.y, v.z}; }
    inline Quaternion ToRaylib(const ecs::Quat& q) { return {q.x, q.y, q.z, q.w}; }
}

// ---------------------------------------------------------------------------
// Physics Configuration (Authoring)
// ---------------------------------------------------------------------------

enum class BodyType { Static, Kinematic, Dynamic };

struct BoxCollider {
    ecs::Vec3 half_extents = {0.5f, 0.5f, 0.5f};
};

struct SphereCollider {
    float radius = 0.5f;
};

// If present, the PhysicsSystem will try to create a Jolt Body for this entity
struct RigidBodyConfig {
    BodyType type = BodyType::Dynamic;
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.0f;
    bool sensor = false;
};

struct CharacterControllerConfig {
    float height = 1.8f;
    float radius = 0.4f;
    float mass = 70.0f;
    float max_slope_angle = 45.0f; // degrees
};

// ---------------------------------------------------------------------------
// Physics Runtime Handles (Managed by Systems)
// ---------------------------------------------------------------------------

// Holds the link to the internal Jolt simulation
struct RigidBodyHandle {
    JPH::BodyID id;
};

// Holds the link to the Jolt CharacterVirtual
struct CharacterHandle {
    // We use a shared_ptr here because CharacterVirtual is a heavy class 
    // that needs manual update/destruction.
    // In a real engine, this might be a handle to a pool.
    std::shared_ptr<JPH::CharacterVirtual> character;
};

// ---------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------

struct MeshRenderer {
    // In a real engine, this would be AssetID mesh_id;
    // For this prototype, we'll store basic shape info or a raw pointer to a Raylib model
    // 0 = Box, 1 = Sphere, 2 = Capsule
    int shape_type = 0; 
    Color color = WHITE;
    ecs::Vec3 scale_offset = {1,1,1}; // Visual scale multiplier
};

// ---------------------------------------------------------------------------
// Gameplay / Input
// ---------------------------------------------------------------------------

struct PlayerInput {
    ecs::Vec2 move_input = {0,0}; // X, Y (WASD / Left Stick)
    ecs::Vec2 look_input = {0,0}; // X, Y (Right Stick)
    bool jump = false;
    bool plant_platform = false;
    float trigger_val = 0.0f;     // Added for axis edge detection
    ecs::Vec3 view_forward = {0,0,1};
    ecs::Vec3 view_right = {1,0,0};
};

struct MainCamera {
    // State
    float orbit_phi = 0.0f;
    float orbit_theta = 0.6f;
    float orbit_distance = 25.0f;
    int zoom_index = 1; // 0=Tight, 1=Medium, 2=Wide
    
    // Smoothing (using Raylib types for direct use in math functions)
    Vector3 lerp_pos = {0, 10, 20};
    Vector3 lerp_target = {0, 0, 0};
    JPH::Vec3 smoothed_vel = JPH::Vec3::sZero();
    
    // Logic state
    float last_manual_move_time = 0.0f;
    bool follow_mode = false;
    
    // Final Output (Consumed by Renderer)
    Camera3D raylib_camera = {0};
};

struct PlayerState {
    int jump_count = 0;
    float air_time = 0.0f;
    float build_cooldown = 0.0f;
    bool trigger_was_down = false;
};

struct PlayerTag {};
struct WorldTag {};
