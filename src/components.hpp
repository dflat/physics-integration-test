#pragma once
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>

// ---------------------------------------------------------------------------
// Render types  (no external library dependencies)
// ---------------------------------------------------------------------------

enum class ShapeType { Box, Sphere, Capsule };

// RGBA colour stored as normalised floats [0, 1].
struct Color4 {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

// Named constants matching the Raylib palette used in the scene.
namespace Colors {
    inline constexpr Color4 White     = {1.000f, 1.000f, 1.000f, 1.0f};
    inline constexpr Color4 LightGray = {0.784f, 0.784f, 0.784f, 1.0f};
    inline constexpr Color4 Gray      = {0.510f, 0.510f, 0.510f, 1.0f};
    inline constexpr Color4 DarkGray  = {0.314f, 0.314f, 0.314f, 1.0f};
    inline constexpr Color4 Red       = {0.902f, 0.161f, 0.216f, 1.0f};
    inline constexpr Color4 Maroon    = {0.745f, 0.129f, 0.216f, 1.0f};
    inline constexpr Color4 Gold      = {1.000f, 0.796f, 0.000f, 1.0f};
    inline constexpr Color4 Orange    = {1.000f, 0.631f, 0.000f, 1.0f};
    inline constexpr Color4 Lime      = {0.000f, 0.620f, 0.184f, 1.0f};
    inline constexpr Color4 DarkGreen = {0.000f, 0.459f, 0.173f, 1.0f};
    inline constexpr Color4 SkyBlue   = {0.400f, 0.749f, 1.000f, 1.0f};
    inline constexpr Color4 DarkBlue  = {0.000f, 0.322f, 0.675f, 1.0f};
    inline constexpr Color4 Purple    = {0.784f, 0.478f, 1.000f, 1.0f};
    inline constexpr Color4 Yellow    = {0.992f, 0.976f, 0.000f, 1.0f};
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
    BodyType type        = BodyType::Dynamic;
    float    mass        = 1.0f;
    float    friction    = 0.5f;
    float    restitution = 0.0f;
    bool     sensor      = false;
};

struct CharacterControllerConfig {
    float height          = 1.8f;
    float radius          = 0.4f;
    float mass            = 70.0f;
    float max_slope_angle = 45.0f; // degrees
};

// ---------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------

struct MeshRenderer {
    ShapeType shape_type   = ShapeType::Box;
    Color4    color        = Colors::White;
    ecs::Vec3 scale_offset = {1, 1, 1};
};

// ---------------------------------------------------------------------------
// Gameplay / Input
// ---------------------------------------------------------------------------

// Semantic hardware intent. Written by PlayerInputSystem; read by CameraSystem
// and CharacterInputSystem. Does NOT store camera-derived view directions.
struct PlayerInput {
    ecs::Vec2 move_input     = {0, 0}; // X, Y (WASD / Left Stick)
    ecs::Vec2 look_input     = {0, 0}; // X, Y (Right Stick)
    bool      jump           = false;
    bool      plant_platform = false;
    float     trigger_val    = 0.0f;
};

// Camera orbit state and smoothing. Also serves as the authoritative source
// for world-space view directions, written by CameraSystem each frame.
struct MainCamera {
    // Orbit parameters
    float orbit_phi      = 0.0f;
    float orbit_theta    = 0.6f;
    float orbit_distance = 25.0f;
    int   zoom_index     = 1; // 0=Tight, 1=Medium, 2=Wide

    // Smoothing buffers (pure data — no engine types)
    ecs::Vec3 lerp_pos    = {0, 10, 20};
    ecs::Vec3 lerp_target = {0,  0,  0};
    ecs::Vec3 smoothed_vel = {0,  0,  0};

    // Logic state
    float last_manual_move_time = 0.0f;
    bool  follow_mode           = false;

    // View directions — written by CameraSystem, read by CharacterInputSystem.
    ecs::Vec3 view_forward = {0, 0, 1};
    ecs::Vec3 view_right   = {1, 0, 0};
};

// Written by CharacterInputSystem; read by CharacterStateSystem and CharacterMotorSystem.
struct CharacterIntent {
    ecs::Vec3 move_dir         = {0, 0, 0};
    ecs::Vec3 look_dir         = {0, 0, 1};
    bool      jump_requested   = false;
    bool      sprint_requested = false;
};

// Written by CharacterStateSystem; read by CharacterMotorSystem.
struct CharacterState {
    enum class Mode { Grounded, Airborne };

    Mode  mode         = Mode::Grounded;
    int   jump_count   = 0;
    float air_time     = 0.0f;
    float jump_impulse = 0.0f;
};

// Builder-specific player state. Owned by PlatformBuilderSystem.
struct PlayerState {
    float build_cooldown   = 0.0f;
    bool  trigger_was_down = false;
};

struct PlayerTag {};
struct WorldTag  {};
