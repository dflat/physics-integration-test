#include "renderer.hpp"
#include "../components.hpp"
#include "../physics_context.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>

using namespace ecs;

void RenderSystem::Update(World& world) {
    // --- Resources ---
    static Shader lighting_shader;
    static bool shader_loaded = false;
    static int lightDirLoc, lightColorLoc, ambientLoc;
    static int playerPosLoc, shadowRadiusLoc, shadowIntensityLoc;

    if (!shader_loaded) {
        lighting_shader = LoadShader("resources/shaders/lighting.vs", "resources/shaders/lighting.fs");
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

    BeginDrawing();
    ClearBackground({35, 35, 40, 255}); 

    // 1. Get Camera and Player Data
    Camera3D camera = {0};
    Vector3 player_pos = {0, 0, 0};
    bool follow_mode = false;

    if (auto* cam_ptr = world.try_resource<MainCamera>()) {
        camera = cam_ptr->raylib_camera;
        follow_mode = cam_ptr->follow_mode;
    }

    world.single<PlayerTag, WorldTransform>([&](Entity, PlayerTag&, WorldTransform& wt) {
        player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };
    });

    // 2. Update Shader Uniforms
    SetShaderValue(lighting_shader, playerPosLoc, &player_pos, SHADER_UNIFORM_VEC3);
    float radius = 0.7f;
    float intensity = 0.5f;
    SetShaderValue(lighting_shader, shadowRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lighting_shader, shadowIntensityLoc, &intensity, SHADER_UNIFORM_FLOAT);

    // 3. Render Scene
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
                    
                    JPH::Vec3 j_fwd = ch->GetRotation() * JPH::Vec3::sAxisZ();
                    JPH::Vec3 j_right = ch->GetRotation() * JPH::Vec3::sAxisX();
                    JPH::Vec3 j_up = ch->GetRotation() * JPH::Vec3::sAxisY();

                    DrawLine3D(pos, Vector3Add(pos, {(float)j_fwd.GetX()*1.5f, (float)j_fwd.GetY()*1.5f, (float)j_fwd.GetZ()*1.5f}), RED);    
                    DrawLine3D(pos, Vector3Add(pos, {(float)j_right.GetX()*1.0f, (float)j_right.GetY()*1.0f, (float)j_right.GetZ()*1.0f}), BLUE); 
                    DrawLine3D(pos, Vector3Add(pos, {(float)j_up.GetX()*1.0f, (float)j_up.GetY()*1.0f, (float)j_up.GetZ()*1.0f}), GREEN);   
                }
            }
        );
        EndShaderMode();
    EndMode3D();

    // 4. Render UI
    DrawFPS(10, 10);
    DrawText("WASD / L-STICK: Move | SPACE / SOUTH: Jump | E,LMB / R-TRIG: Plant Platform", 10, 30, 20, LIGHTGRAY);
    DrawText("R-MOUSE / R-STICK: Orbit | Z,X / L,R-BUMP: Zoom | C / WEST: Toggle Follow", 10, 60, 20, YELLOW);
    
    if (follow_mode) {
        DrawText("CAMERA: FOLLOW MODE", 10, 90, 20, LIME);
    } else {
        DrawText("CAMERA: MANUAL MODE", 10, 90, 20, SKYBLUE);
    }

    EndDrawing();
}
