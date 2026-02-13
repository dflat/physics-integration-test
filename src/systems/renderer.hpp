#pragma once
#include <ecs/ecs.hpp>
#include "../components.hpp"
#include "../physics_context.hpp"
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>

using namespace ecs;

class RenderSystem {
public:
    static void DrawFilledDisk(Vector3 center, float radius, Vector3 normal, Color color) {
        Vector3 v1;
        if (std::abs(normal.y) < 0.99f) v1 = Vector3Normalize(Vector3CrossProduct(normal, {0, 1, 0}));
        else v1 = Vector3Normalize(Vector3CrossProduct(normal, {0, 0, 1}));
        Vector3 v2 = Vector3CrossProduct(normal, v1);

        int segments = 24;
        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);
            for (int i = 0; i < segments; i++) {
                float angle = (float)i * 2.0f * PI / (float)segments;
                float nextAngle = (float)(i + 1) * 2.0f * PI / (float)segments;

                rlVertex3f(center.x, center.y, center.z);
                rlVertex3f(center.x + v1.x * cosf(angle) * radius + v2.x * sinf(angle) * radius,
                           center.y + v1.y * cosf(angle) * radius + v2.y * sinf(angle) * radius,
                           center.z + v1.z * cosf(angle) * radius + v2.z * sinf(angle) * radius);
                rlVertex3f(center.x + v1.x * cosf(nextAngle) * radius + v2.x * sinf(nextAngle) * radius,
                           center.y + v1.y * cosf(nextAngle) * radius + v2.y * sinf(nextAngle) * radius,
                           center.z + v1.z * cosf(nextAngle) * radius + v2.z * sinf(nextAngle) * radius);
            }
        rlEnd();
    }

    static void Update(World& world) {
        float dt = GetFrameTime();
        auto *ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
        if (!ctx_ptr || !*ctx_ptr) return;
        auto &ctx = **ctx_ptr;

        static Shader lighting_shader;
        static bool shader_loaded = false;
        static int lightDirLoc, lightColorLoc, ambientLoc;

        if (!shader_loaded) {
            lighting_shader = LoadShader("/home/rjr/code/projects/physics-integration-test/resources/shaders/lighting.vs", "/home/rjr/code/projects/physics-integration-test/resources/shaders/lighting.fs");
            lightDirLoc = GetShaderLocation(lighting_shader, "lightDir");
            lightColorLoc = GetShaderLocation(lighting_shader, "lightColor");
            ambientLoc = GetShaderLocation(lighting_shader, "ambient");
            Vector3 dir = Vector3Normalize({-0.5f, -1.0f, -0.3f});
            SetShaderValue(lighting_shader, lightDirLoc, &dir, SHADER_UNIFORM_VEC3);
            Vector4 color = {1.0f, 1.0f, 0.9f, 1.0f};
            SetShaderValue(lighting_shader, lightColorLoc, &color, SHADER_UNIFORM_VEC4);
            Vector4 ambient = {0.3f, 0.3f, 0.35f, 1.0f};
            SetShaderValue(lighting_shader, ambientLoc, &ambient, SHADER_UNIFORM_VEC4);
            shader_loaded = true;
        }

        static Vector3 lerp_camera_pos = { 0, 10, 20 };
        static Vector3 lerp_target_pos = { 0, 0, 0 };
        static float orbit_phi = 0.0f;
        static float orbit_theta = 0.6f;
        static float orbit_distance = 25.0f;

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            orbit_phi -= delta.x * 0.005f;
            orbit_theta -= delta.y * 0.005f;
            orbit_theta = std::clamp(orbit_theta, 0.1f, PI * 0.45f);
        }
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.1f) {
            orbit_distance -= wheel * 2.0f;
            orbit_distance = std::clamp(orbit_distance, 5.0f, 80.0f);
        }

        BeginDrawing();
        ClearBackground({35, 35, 40, 255}); 

        Camera3D camera = { 0 };
        camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        Vector3 player_pos = {0};
        bool player_found = false;

        world.single<PlayerTag, WorldTransform, PlayerInput, CharacterHandle>(
            [&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, CharacterHandle& h) {
                player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                player_found = true;
                float x = orbit_distance * sinf(orbit_theta) * sinf(orbit_phi);
                float y = orbit_distance * cosf(orbit_theta);
                float z = orbit_distance * sinf(orbit_theta) * cosf(orbit_phi);
                Vector3 desired_camera = Vector3Add(player_pos, {x, y, z});
                lerp_camera_pos = Vector3Lerp(lerp_camera_pos, desired_camera, 8.0f * dt);
                lerp_target_pos = Vector3Lerp(lerp_target_pos, player_pos, 12.0f * dt);
                camera.position = lerp_camera_pos;
                camera.target = lerp_target_pos;
                Vector3 fwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 right = Vector3CrossProduct(fwd, camera.up);
                input.view_forward = {fwd.x, fwd.y, fwd.z};
                input.view_right = {right.x, right.y, right.z};
            }
        );

        BeginMode3D(camera);
            DrawGrid(100, 2.0f);

            // --- 1. Opaque Pass (Entities) ---
            BeginShaderMode(lighting_shader);
            world.each<WorldTransform, MeshRenderer>(
                [&](Entity, WorldTransform& wt, MeshRenderer& mesh) {
                    rlPushMatrix();
                    rlMultMatrixf((float*)&wt.matrix);
                    switch (mesh.shape_type) {
                        case 0: DrawCube({0,0,0}, 1.0f, 1.0f, 1.0f, mesh.color); break;
                        case 1: DrawSphere({0,0,0}, 0.5f, mesh.color); break;
                        case 2: DrawCapsule({0,0,0}, {0, 1.8f, 0}, 0.4f, 8, 8, mesh.color); break;
                    }
                    rlPopMatrix();
                }
            );
            EndShaderMode();

            // --- 2. Transparent Pass (Shadows) ---
            if (player_found) {
                JPH::RRayCast ray;
                ray.mOrigin = JPH::RVec3(player_pos.x, player_pos.y + 0.1f, player_pos.z);
                ray.mDirection = JPH::Vec3(0, -40.0f, 0);
                JPH::RayCastResult hit;
                JPH::SpecifiedBroadPhaseLayerFilter bp_filter(BroadPhaseLayers::NON_MOVING);
                JPH::SpecifiedObjectLayerFilter obj_filter(Layers::NON_MOVING);

                if (ctx.physics_system->GetNarrowPhaseQuery().CastRay(ray, hit, bp_filter, obj_filter)) {
                    JPH::RVec3 hit_pos = ray.GetPointOnRay(hit.mFraction);
                    JPH::Vec3 hit_normal(0, 1, 0);

                    JPH::BodyLockRead lock(ctx.physics_system->GetBodyLockInterface(), hit.mBodyID);
                    if (lock.Succeeded()) {
                        hit_normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hit_pos);
                    }
                    
                    float distance = 40.0f * hit.mFraction;
                    Vector3 shadow_center = { (float)hit_pos.GetX(), (float)hit_pos.GetY() + 0.05f, (float)hit_pos.GetZ() };
                    Vector3 normal = { (float)hit_normal.GetX(), (float)hit_normal.GetY(), (float)hit_normal.GetZ() };
                    
                    float alpha_factor = std::clamp(1.0f - (distance / 15.0f), 0.0f, 1.0f);
                    float size_factor = 0.6f + (0.4f * alpha_factor); 
                    Color shadow_color = { 10, 10, 15, (unsigned char)(180 * alpha_factor) };
                    
                    rlDisableBackfaceCulling();
                    rlDisableDepthMask(); // Don't write shadow to depth buffer
                    DrawFilledDisk(shadow_center, 0.6f * size_factor, normal, shadow_color);
                    rlEnableDepthMask();
                    rlEnableBackfaceCulling();
                }
            }

        EndMode3D();

        DrawFPS(10, 10);
        DrawText("WASD: Move | SPACE: Jump (Double) | R: Reset", 10, 30, 20, LIGHTGRAY);
        DrawText("RIGHT MOUSE: Orbit (Inverted) | SCROLL: Zoom", 10, 60, 20, YELLOW);
        EndDrawing();
    }
};
