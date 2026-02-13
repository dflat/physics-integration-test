#pragma once
#include <ecs/ecs.hpp>
#include "../components.hpp"
#include "../physics_context.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>

using namespace ecs;

class RenderSystem {
public:
    static void Update(World& world) {
        float dt = GetFrameTime();
        
        // --- Resources (Lazy Init) ---
        static Shader lighting_shader;
        static bool shader_loaded = false;
        static int lightDirLoc, lightColorLoc, ambientLoc;

        if (!shader_loaded) {
            lighting_shader = LoadShader("/home/rjr/code/projects/physics-integration-test/resources/shaders/lighting.vs", "/home/rjr/code/projects/physics-integration-test/resources/shaders/lighting.fs");
            lightDirLoc = GetShaderLocation(lighting_shader, "lightDir");
            lightColorLoc = GetShaderLocation(lighting_shader, "lightColor");
            ambientLoc = GetShaderLocation(lighting_shader, "ambient");
            
            // Set some defaults for a nice "afternoon" look
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

        // Input
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            orbit_phi -= delta.x * 0.005f;
            orbit_theta += delta.y * 0.005f;
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

        world.single<PlayerTag, WorldTransform, PlayerInput, CharacterHandle>(
            [&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, CharacterHandle& h) {
                Vector3 player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                
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

            // Render Entities with Lighting
            BeginShaderMode(lighting_shader);
            world.each<WorldTransform, MeshRenderer>(
                [&](Entity, WorldTransform& wt, MeshRenderer& mesh) {
                    Vector3 pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
                    float sx = Vector3Length({wt.matrix.m[0], wt.matrix.m[1], wt.matrix.m[2]});
                    float sy = Vector3Length({wt.matrix.m[4], wt.matrix.m[5], wt.matrix.m[6]});
                    float sz = Vector3Length({wt.matrix.m[8], wt.matrix.m[9], wt.matrix.m[10]});

                    switch (mesh.shape_type) {
                        case 0: // Box
                            DrawCube(pos, sx, sy, sz, mesh.color);
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
            EndShaderMode();
        EndMode3D();

        DrawFPS(10, 10);
        DrawText("WASD: Move | SPACE: Jump | R: Reset", 10, 30, 20, LIGHTGRAY);
        DrawText("RIGHT MOUSE: Orbit | SCROLL: Zoom", 10, 60, 20, YELLOW);
        
        EndDrawing();
    }
};
