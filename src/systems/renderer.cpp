#include "renderer.hpp"
#include "../components.hpp"
#include "../assets.hpp"
#include "../physics_context.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>

using namespace ecs;

// Convert our engine Color4 to Raylib's Color at draw time.
static inline Color to_raylib(const Color4& c) {
    return Color{
        static_cast<unsigned char>(c.r * 255.0f),
        static_cast<unsigned char>(c.g * 255.0f),
        static_cast<unsigned char>(c.b * 255.0f),
        static_cast<unsigned char>(c.a * 255.0f),
    };
}

void RenderSystem::Update(World& world) {
    auto* assets = world.try_resource<AssetResource>();
    if (!assets) return;

    BeginDrawing();
    ClearBackground({35, 35, 40, 255});

    // 1. Build Camera3D from MainCamera data and collect player position
    Camera3D camera = {};
    Vector3  player_pos  = {0, 0, 0};
    bool     follow_mode = false;

    if (auto* cam = world.try_resource<MainCamera>()) {
        camera.position   = {cam->lerp_pos.x,    cam->lerp_pos.y,    cam->lerp_pos.z};
        camera.target     = {cam->lerp_target.x,  cam->lerp_target.y, cam->lerp_target.z};
        camera.up         = {0, 1, 0};
        camera.fovy       = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        follow_mode = cam->follow_mode;
    }

    world.single<PlayerTag, WorldTransform>([&](Entity, PlayerTag&, WorldTransform& wt) {
        player_pos = {wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14]};
    });

    // 2. Update Shader Uniforms
    SetShaderValue(assets->lighting_shader, assets->playerPosLoc,
                   &player_pos, SHADER_UNIFORM_VEC3);
    float radius    = 0.7f;
    float intensity = 0.5f;
    SetShaderValue(assets->lighting_shader, assets->shadowRadiusLoc,    &radius,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(assets->lighting_shader, assets->shadowIntensityLoc, &intensity, SHADER_UNIFORM_FLOAT);

    // 3. Render Scene
    BeginMode3D(camera);
        DrawGrid(100, 2.0f);
        BeginShaderMode(assets->lighting_shader);
        world.each<WorldTransform, MeshRenderer>(
            [&](Entity e, WorldTransform& wt, MeshRenderer& mesh) {
                rlPushMatrix();
                rlMultMatrixf((float*)&wt.matrix);
                Color col = to_raylib(mesh.color);
                switch (mesh.shape_type) {
                    case ShapeType::Box:     DrawCube({0,0,0}, 1.0f, 1.0f, 1.0f, col); break;
                    case ShapeType::Sphere:  DrawSphere({0,0,0}, 0.5f, col);            break;
                    case ShapeType::Capsule: DrawCapsule({0,0,0}, {0, 1.8f, 0}, 0.4f, 8, 8, col); break;
                }
                rlPopMatrix();

                // Draw orientation gizmo on the player
                if (world.has<PlayerTag>(e)) {
                    auto* h_ptr = world.try_get<CharacterHandle>(e);
                    if (!h_ptr) return;
                    auto& ch = h_ptr->character;

                    Vector3 pos = {wt.matrix.m[12], wt.matrix.m[13] + 1.0f, wt.matrix.m[14]};

                    JPH::Vec3 j_fwd   = ch->GetRotation() * JPH::Vec3::sAxisZ();
                    JPH::Vec3 j_right = ch->GetRotation() * JPH::Vec3::sAxisX();
                    JPH::Vec3 j_up    = ch->GetRotation() * JPH::Vec3::sAxisY();

                    DrawLine3D(pos, Vector3Add(pos, {j_fwd.GetX()*1.5f,   j_fwd.GetY()*1.5f,   j_fwd.GetZ()*1.5f}),   RED);
                    DrawLine3D(pos, Vector3Add(pos, {j_right.GetX()*1.0f, j_right.GetY()*1.0f, j_right.GetZ()*1.0f}), BLUE);
                    DrawLine3D(pos, Vector3Add(pos, {j_up.GetX()*1.0f,    j_up.GetY()*1.0f,    j_up.GetZ()*1.0f}),    GREEN);
                }
            });
        EndShaderMode();
    EndMode3D();

    // 4. Render UI
    DrawFPS(10, 10);
    DrawText("WASD / L-STICK: Move | SPACE / SOUTH: Jump | E,LMB / R-TRIG: Plant Platform", 10, 30, 20, LIGHTGRAY);
    DrawText("R-MOUSE / R-STICK: Orbit | Z,X / L,R-BUMP: Zoom | C / WEST: Toggle Follow",   10, 60, 20, YELLOW);

    DrawText(follow_mode ? "CAMERA: FOLLOW MODE" : "CAMERA: MANUAL MODE",
             10, 90, 20, follow_mode ? GREEN : SKYBLUE);

    EndDrawing();
}
