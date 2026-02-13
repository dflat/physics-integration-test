#pragma once
#include <ecs/ecs.hpp>
#include "../components.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>

using namespace ecs;

class RenderSystem {
public:
    static void Update(World& world) {
        float dt = GetFrameTime();
        
        // --- Persistent Camera State ---
        static Vector3 lerp_camera_pos = { 0, 10, 20 };
        static Vector3 lerp_target_pos = { 0, 0, 0 };
        
        // Orbit parameters
        static float orbit_phi = 0.0f;    // Horizontal angle
        static float orbit_theta = 0.4f;  // Vertical angle (radians)
        static float orbit_distance = 20.0f;

        // 1. Handle Camera Input
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            orbit_phi -= delta.x * 0.005f;
            orbit_theta += delta.y * 0.005f;
            
            // Constrain vertical orbit to avoid gimbal lock/flipping
            orbit_theta = std::clamp(orbit_theta, 0.1f, PI * 0.45f);
        }

        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.1f) {
            orbit_distance -= wheel * 2.0f;
            orbit_distance = std::clamp(orbit_distance, 5.0f, 60.0f);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        Camera3D camera = { 0 };
        camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        // Find player
        world.single<PlayerTag, WorldTransform, PlayerInput, CharacterHandle>(
            [&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, CharacterHandle& h) {
                Vector3 player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                
                // 2. Calculate Desired Camera Position (Orbit)
                // Convert spherical to cartesian
                float x = orbit_distance * sinf(orbit_theta) * sinf(orbit_phi);
                float y = orbit_distance * cosf(orbit_theta);
                float z = orbit_distance * sinf(orbit_theta) * cosf(orbit_phi);
                
                Vector3 desired_offset = { x, y, z };
                Vector3 desired_camera = Vector3Add(player_pos, desired_offset);
                Vector3 desired_target = player_pos;

                // 3. Apply Lazy Follow (Smoothing)
                float camera_speed = 8.0f; 
                float target_speed = 12.0f;

                lerp_camera_pos = Vector3Lerp(lerp_camera_pos, desired_camera, camera_speed * dt);
                lerp_target_pos = Vector3Lerp(lerp_target_pos, desired_target, target_speed * dt);

                camera.position = lerp_camera_pos;
                camera.target = lerp_target_pos;
                
                // Update View vectors for Input
                Vector3 fwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 right = Vector3CrossProduct(fwd, camera.up);
                input.view_forward = {fwd.x, fwd.y, fwd.z};
                input.view_right = {right.x, right.y, right.z};

                // HUD
                auto* ch = h.character.get();
                auto ground_state = ch->GetGroundState();
                const char* ground_str = (ground_state == JPH::CharacterVirtual::EGroundState::OnGround) ? "OnGround" : "InAir";
                
                DrawText(TextFormat("Ground: %s", ground_str), 10, 60, 20, WHITE);
                DrawText(TextFormat("Orbit: %.2f, %.2f Dist: %.1f", orbit_phi, orbit_theta, orbit_distance), 10, 80, 20, WHITE);
            }
        );

        BeginMode3D(camera);
            DrawGrid(100, 2.0f);

            // Render Entities
            world.each<WorldTransform, MeshRenderer>(
                [&](Entity, WorldTransform& wt, MeshRenderer& mesh) {
                    Vector3 pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                    float sx = Vector3Length({wt.matrix.m[0], wt.matrix.m[1], wt.matrix.m[2]});
                    float sy = Vector3Length({wt.matrix.m[4], wt.matrix.m[5], wt.matrix.m[6]});
                    float sz = Vector3Length({wt.matrix.m[8], wt.matrix.m[9], wt.matrix.m[10]});

                    switch (mesh.shape_type) {
                        case 0: // Box
                            DrawCube(pos, 1.0f * sx, 1.0f * sy, 1.0f * sz, mesh.color);
                            DrawCubeWires(pos, 1.0f * sx, 1.0f * sy, 1.0f * sz, DARKGRAY);
                            break;
                        case 1: // Sphere
                            DrawSphere(pos, 0.5f * sx, mesh.color);
                            break;
                        case 2: // Capsule (Player)
                            DrawCapsule(pos, Vector3Add(pos, {0, 1.8f * sy, 0}), 0.4f * sx, 8, 8, mesh.color);
                            break;
                    }
                }
            );
        EndMode3D();

        DrawFPS(10, 10);
        DrawText("WASD: Move | SPACE: Jump | R: Reset", 10, 30, 20, LIGHTGRAY);
        DrawText("RIGHT MOUSE: Orbit | SCROLL: Zoom", 10, 120, 20, YELLOW);
        
        EndDrawing();
    }
};
