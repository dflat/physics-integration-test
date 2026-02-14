#pragma once
#include "../components.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>
#include <raymath.h>
#include <algorithm>

using namespace ecs;

class CameraSystem {
public:
    static void Update(World& world, float dt) {
        // Find player input
        PlayerInput* player_input = nullptr;
        world.single<PlayerTag, PlayerInput>([&](Entity, PlayerTag&, PlayerInput& input) {
            player_input = &input;
        });

        if (!player_input) return;

        world.each<MainCamera>([&](Entity, MainCamera& cam) {
            // 1. Handle Input (Toggle & Manual Move)
            if (IsKeyPressed(KEY_C)) cam.follow_mode = !cam.follow_mode;

            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                Vector2 delta = GetMouseDelta();
                cam.orbit_phi -= delta.x * 0.005f;
                cam.orbit_theta -= delta.y * 0.005f;
                cam.orbit_theta = std::clamp(cam.orbit_theta, 0.1f, PI * 0.45f);
                cam.last_manual_move_time = 0.0f;
            } else {
                cam.last_manual_move_time += dt;
            }

            float wheel = GetMouseWheelMove();
            if (std::abs(wheel) > 0.1f) {
                cam.orbit_distance -= wheel * 2.0f;
                cam.orbit_distance = std::clamp(cam.orbit_distance, 5.0f, 80.0f);
                cam.last_manual_move_time = 0.0f;
            }

            // 2. Follow Logic
            world.single<PlayerTag, WorldTransform, CharacterHandle>([&](Entity, PlayerTag&, WorldTransform& wt, CharacterHandle& h) {
                Vector3 player_pos = { wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14] };

                if (cam.follow_mode && cam.last_manual_move_time > 1.0f) {
                    JPH::Vec3 vel = h.character->GetLinearVelocity();
                    vel.SetY(0);
                    cam.smoothed_vel += (vel - cam.smoothed_vel) * 5.0f * dt;

                    float speed_sq = cam.smoothed_vel.LengthSq();
                    if (speed_sq > 0.5f) {
                        JPH::Vec3 move_dir = cam.smoothed_vel.Normalized();
                        JPH::Vec3 cam_to_player(-sinf(cam.orbit_phi), 0, -cosf(cam.orbit_phi));
                        float alignment = move_dir.Dot(cam_to_player);

                        if (alignment > 0.0f) {
                            float target_phi = atan2f(-move_dir.GetX(), -move_dir.GetZ());
                            float diff = target_phi - cam.orbit_phi;
                            while (diff < -PI) diff += 2 * PI;
                            while (diff > PI) diff -= 2 * PI;

                            float alignment_weight = std::clamp(alignment, 0.0f, 1.0f);
                            float speed_factor = std::clamp(sqrtf(speed_sq) / 10.0f, 0.0f, 1.0f);
                            cam.orbit_phi += diff * 5.0f * alignment_weight * speed_factor * dt;
                        }
                    }
                    
                    cam.orbit_theta = Lerp(cam.orbit_theta, 1.1f, 2.0f * dt);
                    cam.orbit_distance = Lerp(cam.orbit_distance, 15.0f, 2.0f * dt);
                }

                // 3. Finalize Transform
                float x = cam.orbit_distance * sinf(cam.orbit_theta) * sinf(cam.orbit_phi);
                float y = cam.orbit_distance * cosf(cam.orbit_theta);
                float z = cam.orbit_distance * sinf(cam.orbit_theta) * cosf(cam.orbit_phi);

                cam.lerp_pos = Vector3Lerp(cam.lerp_pos, Vector3Add(player_pos, {x, y, z}), 8.0f * dt);
                cam.lerp_target = Vector3Lerp(cam.lerp_target, player_pos, 12.0f * dt);

                cam.raylib_camera.position = cam.lerp_pos;
                cam.raylib_camera.target = cam.lerp_target;
                cam.raylib_camera.up = {0, 1, 0};
                cam.raylib_camera.fovy = 45.0f;
                cam.raylib_camera.projection = CAMERA_PERSPECTIVE;

                // Sync back to input for movement system
                Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.raylib_camera.target, cam.raylib_camera.position));
                Vector3 right = Vector3CrossProduct(fwd, cam.raylib_camera.up);
                player_input->view_forward = {fwd.x, fwd.y, fwd.z};
                player_input->view_right = {right.x, right.y, right.z};
            });
        });
    }
};
