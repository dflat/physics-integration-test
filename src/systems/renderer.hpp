#pragma once
#include <ecs/ecs.hpp>
#include "../components.hpp"
#include "../physics_context.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>

using namespace ecs;

class RenderSystem {
public:
    static void Update(World& world) {
        float dt = GetFrameTime();
        
        // --- Resources ---
        static Shader lighting_shader;
        static bool shader_loaded = false;
        static int lightDirLoc, lightColorLoc, ambientLoc;
        static int playerPosLoc, shadowRadiusLoc, shadowIntensityLoc;

        if (!shader_loaded) {
            lighting_shader = LoadShader("/home/rjr/code/projects/physics-integration-test/resources/shaders/lighting.vs", "/home/rjr/code/projects/physics-integration-test/resources/shaders/lighting.fs");
            lightDirLoc = GetShaderLocation(lighting_shader, "lightDir");
            lightColorLoc = GetShaderLocation(lighting_shader, "lightColor");
            ambientLoc = GetShaderLocation(lighting_shader, "ambient");
            playerPosLoc = GetShaderLocation(lighting_shader, "playerPos");
            shadowRadiusLoc = GetShaderLocation(lighting_shader, "shadowRadius");
            shadowIntensityLoc = GetShaderLocation(lighting_shader, "shadowIntensity");

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
        static float last_manual_move_time = 0.0f;
        static JPH::Vec3 smoothed_vel = JPH::Vec3::sZero();
        
        last_manual_move_time += dt;
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.1f) {
            orbit_distance -= wheel * 2.0f;
            orbit_distance = std::clamp(orbit_distance, 5.0f, 80.0f);
            last_manual_move_time = 0.0f;
        }

        BeginDrawing();
        ClearBackground({35, 35, 40, 255}); 

        Camera3D camera = { 0 };
        camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        Vector3 player_pos = {0, -100, 0};

        world.single<PlayerTag, WorldTransform, PlayerInput, CharacterHandle>(
            [&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, CharacterHandle& h) {
                player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    Vector2 delta = GetMouseDelta();
                    orbit_phi -= delta.x * 0.005f;
                    orbit_theta -= delta.y * 0.005f;
                    orbit_theta = std::clamp(orbit_theta, 0.1f, PI * 0.45f);
                    last_manual_move_time = 0.0f;
                }

                if (input.camera_follow_mode && last_manual_move_time > 1.0f) {
                    // Lazy Follow: Stay behind the character's movement direction
                    JPH::Vec3 vel = h.character->GetLinearVelocity();
                    vel.SetY(0);
                    
                    // Smooth the velocity vector - increased speed (2.0 -> 5.0)
                    smoothed_vel += (vel - smoothed_vel) * 5.0f * dt;

                    // Always calculate a target_phi based on where we WANT to be.
                    // If moving, we update it. If stopped, we look at character's rotation.
                    static float target_phi = 0.0f;

                    float speed_sq = smoothed_vel.LengthSq();
                    if (speed_sq > 0.1f) {
                        target_phi = atan2f(-smoothed_vel.GetX(), -smoothed_vel.GetZ());
                    } else {
                        // If stopped, stay behind the character's actual forward vector
                        JPH::Vec3 ch_fwd = h.character->GetRotation() * JPH::Vec3::sAxisZ();
                        target_phi = atan2f(-ch_fwd.GetX(), -ch_fwd.GetZ());
                    }

                    // Calculate difference and normalize to [-PI, PI]
                    float diff = target_phi - orbit_phi;
                    while (diff < -PI) diff += 2 * PI;
                    while (diff > PI) diff -= 2 * PI;

                    // Interpolation speed: significantly snappier (1.5 -> 4.0)
                    float speed_factor = std::clamp(sqrtf(speed_sq) / 10.0f, 0.2f, 1.0f);
                    float follow_speed = 4.0f * speed_factor;
                    
                    orbit_phi += diff * follow_speed * dt;
                    
                    // Smoothly return to a standard 3rd person pitch/distance
                    orbit_theta = Lerp(orbit_theta, 1.1f, 2.0f * dt);
                    orbit_distance = Lerp(orbit_distance, 15.0f, 2.0f * dt);
                } 

                float x = orbit_distance * sinf(orbit_theta) * sinf(orbit_phi);
                float y = orbit_distance * cosf(orbit_theta);
                float z = orbit_distance * sinf(orbit_theta) * cosf(orbit_phi);

                lerp_camera_pos = Vector3Lerp(lerp_camera_pos, Vector3Add(player_pos, {x, y, z}), 8.0f * dt);
                lerp_target_pos = Vector3Lerp(lerp_target_pos, player_pos, 12.0f * dt);
                camera.position = lerp_camera_pos;
                camera.target = lerp_target_pos;
                Vector3 fwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 right = Vector3CrossProduct(fwd, camera.up);
                input.view_forward = {fwd.x, fwd.y, fwd.z};
                input.view_right = {right.x, right.y, right.z};
            }
        );

        // Update Uniforms
        SetShaderValue(lighting_shader, playerPosLoc, &player_pos, SHADER_UNIFORM_VEC3);
        float radius = 0.7f;
        float intensity = 0.5f;
        SetShaderValue(lighting_shader, shadowRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lighting_shader, shadowIntensityLoc, &intensity, SHADER_UNIFORM_FLOAT);

        BeginMode3D(camera);
            DrawGrid(100, 2.0f);
            BeginShaderMode(lighting_shader);
            world.each<WorldTransform, MeshRenderer>(
                [&](Entity e, WorldTransform& wt, MeshRenderer& mesh) {
                    rlPushMatrix();
                    rlMultMatrixf((float*)&wt.matrix);
                    switch (mesh.shape_type) {
                        case 0: DrawCube({0,0,0}, 1.0f, 1.0f, 1.0f, mesh.color); break;
                        case 1: DrawSphere({0,0,0}, 0.5f, mesh.color); break;
                        case 2: DrawCapsule({0,0,0}, {0, 1.8f, 0}, 0.4f, 8, 8, mesh.color); break;
                    }
                    rlPopMatrix();

                    // Draw Gizmo for player
                    if (world.has<PlayerTag>(e)) {
                        auto* h_ptr = world.try_get<CharacterHandle>(e);
                        if (!h_ptr) return;
                        auto& ch = h_ptr->character;

                        Vector3 pos = { wt.matrix.m[12], wt.matrix.m[13] + 1.0f, wt.matrix.m[14] };
                        
                        // Use actual Jolt rotation for gizmo
                        JPH::Vec3 j_fwd = ch->GetRotation() * JPH::Vec3::sAxisZ();
                        JPH::Vec3 j_right = ch->GetRotation() * JPH::Vec3::sAxisX();
                        JPH::Vec3 j_up = ch->GetRotation() * JPH::Vec3::sAxisY();

                        DrawLine3D(pos, Vector3Add(pos, {(float)j_fwd.GetX()*1.5f, (float)j_fwd.GetY()*1.5f, (float)j_fwd.GetZ()*1.5f}), RED);    // Forward (Z)
                        DrawLine3D(pos, Vector3Add(pos, {(float)j_right.GetX()*1.0f, (float)j_right.GetY()*1.0f, (float)j_right.GetZ()*1.0f}), BLUE); // Right (X)
                        DrawLine3D(pos, Vector3Add(pos, {(float)j_up.GetX()*1.0f, (float)j_up.GetY()*1.0f, (float)j_up.GetZ()*1.0f}), GREEN);   // Up (Y)
                    }
                }
            );
            EndShaderMode();
        EndMode3D();

        DrawFPS(10, 10);
        DrawText("WASD: Move | SPACE: Jump (Double) | R: Reset", 10, 30, 20, LIGHTGRAY);
        DrawText("RIGHT MOUSE: Orbit (Inverted) | SCROLL: Zoom | C: Toggle Follow", 10, 60, 20, YELLOW);
        
        world.single<PlayerInput>([&](Entity, PlayerInput& input) {
            if (input.camera_follow_mode) {
                DrawText("CAMERA: FOLLOW MODE", 10, 90, 20, LIME);
            } else {
                DrawText("CAMERA: MANUAL MODE", 10, 90, 20, SKYBLUE);
            }
        });

        EndDrawing();
    }
};
