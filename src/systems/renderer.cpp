#include "renderer.hpp"
#include "../components.hpp"
#include "../assets.hpp"
#include "../physics_context.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>

using namespace ecs;

void RenderSystem::Update(World& world) {
    auto* assets = world.try_resource<AssetResource>();
    if (!assets) return;

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
    SetShaderValue(assets->lighting_shader, assets->playerPosLoc, &player_pos, SHADER_UNIFORM_VEC3);
    float radius = 0.7f;
    float intensity = 0.5f;
    SetShaderValue(assets->lighting_shader, assets->shadowRadiusLoc, &radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(assets->lighting_shader, assets->shadowIntensityLoc, &intensity, SHADER_UNIFORM_FLOAT);

    // 3. Render Scene
    BeginMode3D(camera);
        DrawGrid(100, 2.0f);
        BeginShaderMode(assets->lighting_shader);
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
