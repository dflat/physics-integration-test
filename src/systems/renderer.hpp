#pragma once
#include <ecs/ecs.hpp>
#include "../components.hpp"
#include <raylib.h>
#include <raymath.h>

using namespace ecs;

class RenderSystem {
public:
    static void Update(World& world) {
        float dt = GetFrameTime();
        
        // Persistent camera state using a static variable (simple for prototype)
        static Vector3 lerp_camera_pos = { 0, 10, 20 };
        static Vector3 lerp_target_pos = { 0, 0, 0 };

        BeginDrawing();
        ClearBackground(DARKGRAY);

        Camera3D camera = { 0 };
        camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        // Find player to focus camera and display HUD
        world.single<PlayerTag, WorldTransform, PlayerInput, CharacterHandle>(
            [&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, CharacterHandle& h) {
                Vector3 player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                
                // --- Lazy Follow Logic ---
                // Target is player position, Camera is offset behind
                Vector3 desired_target = player_pos;
                Vector3 desired_camera = Vector3Add(player_pos, Vector3{0, 8, 15}); // Pulled back further (8 up, 15 back)

                // Snappiness factors (higher = faster)
                float camera_speed = 5.0f; 
                float target_speed = 10.0f;

                lerp_camera_pos = Vector3Lerp(lerp_camera_pos, desired_camera, camera_speed * dt);
                lerp_target_pos = Vector3Lerp(lerp_target_pos, desired_target, target_speed * dt);

                camera.position = lerp_camera_pos;
                camera.target = lerp_target_pos;
                
                // Update View vectors for Input (crucial for movement relative to camera)
                Vector3 fwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 right = Vector3CrossProduct(fwd, camera.up);
                input.view_forward = {fwd.x, fwd.y, fwd.z};
                input.view_right = {right.x, right.y, right.z};

                // HUD
                auto* ch = h.character.get();
                auto ground_state = ch->GetGroundState();
                const char* ground_str = "Unknown";
                switch(ground_state) {
                    case JPH::CharacterVirtual::EGroundState::OnGround: ground_str = "OnGround"; break;
                    case JPH::CharacterVirtual::EGroundState::InAir: ground_str = "InAir"; break;
                    default: ground_str = "Other"; break;
                }
                
                JPH::Vec3 vel = ch->GetLinearVelocity();
                DrawText(TextFormat("Ground: %s", ground_str), 10, 60, 20, WHITE);
                DrawText(TextFormat("Vel: %.2f, %.2f, %.2f", vel.GetX(), vel.GetY(), vel.GetZ()), 10, 80, 20, WHITE);
                DrawText(TextFormat("Pos: %.2f, %.2f, %.2f", player_pos.x, player_pos.y, player_pos.z), 10, 100, 20, WHITE);
            }
        );

        BeginMode3D(camera);
            DrawGrid(50, 2.0f);

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
        DrawText("WASD: Move (Accelerated) | SPACE: Jump", 10, 30, 20, LIGHTGRAY);
        
        EndDrawing();
    }
};
