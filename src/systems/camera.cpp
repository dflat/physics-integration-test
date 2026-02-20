#include "camera.hpp"
#include "../components.hpp"
#include "../physics_handles.hpp"
#include "../math_util.hpp"
#include "../input_state.hpp"
#include <ecs/modules/transform.hpp>
#include <raylib.h>
#include <raymath.h>
#include <algorithm>

using namespace ecs;

// Local helpers: ecs::Vec3 <-> Raylib Vector3
static inline Vector3 to_v3(const ecs::Vec3& v) { return {v.x, v.y, v.z}; }
static inline ecs::Vec3 from_v3(const Vector3& v) { return {v.x, v.y, v.z}; }

void CameraSystem::Update(World& world, float dt) {
    // 1. Get Resources
    auto* input_ptr = world.try_resource<InputRecord>();
    if (!input_ptr) return;
    const auto& record = *input_ptr;

    PlayerInput* player_input = nullptr;
    world.single<PlayerTag, PlayerInput>([&](Entity, PlayerTag&, PlayerInput& input) {
        player_input = &input;
    });

    auto* cam_ptr = world.try_resource<MainCamera>();
    if (!player_input || !cam_ptr) return;
    MainCamera& cam = *cam_ptr;

    // 2. Handle Input (Toggle & Manual Move)
    bool toggle_follow = record.keys_pressed[KEY_C];

    int zoom_delta = 0;
    if (record.keys_pressed[KEY_X]) zoom_delta++;
    if (record.keys_pressed[KEY_Z]) zoom_delta--;

    for (const auto& gp : record.gamepads) {
        if (gp.buttons_pressed[GAMEPAD_BUTTON_RIGHT_FACE_LEFT]) toggle_follow = true;
        if (gp.buttons_pressed[GAMEPAD_BUTTON_LEFT_TRIGGER_1])  zoom_delta--;
        if (gp.buttons_pressed[GAMEPAD_BUTTON_RIGHT_TRIGGER_1]) zoom_delta++;
    }
    if (toggle_follow) cam.follow_mode = !cam.follow_mode;

    if (zoom_delta != 0) {
        cam.zoom_index = std::clamp(cam.zoom_index + zoom_delta, 0, 2);
        cam.last_manual_move_time = 0.0f;
    }

    static const float zoom_levels[] = {10.0f, 25.0f, 50.0f};
    cam.orbit_distance = Lerp(cam.orbit_distance, zoom_levels[cam.zoom_index], 5.0f * dt);

    // Manual Mouse Orbit
    if (record.mouse_buttons[MOUSE_BUTTON_RIGHT]) {
        cam.orbit_phi   -= record.mouse_delta.x * 0.005f;
        cam.orbit_theta -= record.mouse_delta.y * 0.005f;
        cam.last_manual_move_time = 0.0f;
    }
    // Manual Gamepad Orbit
    else if (std::abs(player_input->look_input.x) > 0.01f ||
             std::abs(player_input->look_input.y) > 0.01f) {
        cam.orbit_phi   -= player_input->look_input.x * 2.5f * dt;
        cam.orbit_theta += player_input->look_input.y * 2.5f * dt;
        cam.last_manual_move_time = 0.0f;
    }
    else {
        cam.last_manual_move_time += dt;
    }

    cam.orbit_theta = std::clamp(cam.orbit_theta, 0.1f, PI * 0.45f);

    if (std::abs(record.mouse_wheel) > 0.1f) {
        cam.orbit_distance -= record.mouse_wheel * 2.0f;
        cam.orbit_distance  = std::clamp(cam.orbit_distance, 5.0f, 80.0f);
        cam.last_manual_move_time = 0.0f;
    }

    // 3. Follow Logic
    world.single<PlayerTag, WorldTransform, CharacterHandle>(
        [&](Entity, PlayerTag&, WorldTransform& wt, CharacterHandle& h) {
            Vector3 player_pos = {wt.matrix.m[12], wt.matrix.m[13], wt.matrix.m[14]};

            if (cam.follow_mode && cam.last_manual_move_time > 1.0f) {
                // Smooth the character's horizontal velocity for stable follow
                JPH::Vec3 vel = h.character->GetLinearVelocity();
                vel.SetY(0);
                JPH::Vec3 sv = MathBridge::ToJolt(cam.smoothed_vel);
                sv += (vel - sv) * 5.0f * dt;
                cam.smoothed_vel = MathBridge::FromJolt(sv);

                static float target_phi = 0.0f;

                float speed_sq = sv.LengthSq();
                if (speed_sq > 0.1f) {
                    JPH::Vec3 move_dir     = sv.Normalized();
                    JPH::Vec3 cam_to_player(-sinf(cam.orbit_phi), 0, -cosf(cam.orbit_phi));
                    float alignment = engine::math::calculate_alignment(
                        move_dir.GetX(), move_dir.GetZ(),
                        cam_to_player.GetX(), cam_to_player.GetZ());

                    if (alignment > 0.0f) {
                        target_phi = engine::math::calculate_follow_angle(
                            move_dir.GetX(), move_dir.GetZ());
                        float diff = engine::math::normalize_angle(target_phi - cam.orbit_phi);

                        float aw = std::clamp(alignment, 0.0f, 1.0f);
                        float sf = std::clamp(sqrtf(speed_sq) / 10.0f, 0.0f, 1.0f);
                        cam.orbit_phi += diff * 5.0f * aw * sf * dt;
                    }
                } else {
                    JPH::Vec3 ch_fwd = h.character->GetRotation() * JPH::Vec3::sAxisZ();
                    target_phi = engine::math::calculate_follow_angle(ch_fwd.GetX(), ch_fwd.GetZ());
                    cam.orbit_phi += engine::math::normalize_angle(target_phi - cam.orbit_phi) * 1.0f * dt;
                }

                cam.orbit_theta = Lerp(cam.orbit_theta, 1.1f, 2.0f * dt);
            }

            // 4. Finalize position via lerp
            float x = cam.orbit_distance * sinf(cam.orbit_theta) * sinf(cam.orbit_phi);
            float y = cam.orbit_distance * cosf(cam.orbit_theta);
            float z = cam.orbit_distance * sinf(cam.orbit_theta) * cosf(cam.orbit_phi);

            cam.lerp_pos    = from_v3(Vector3Lerp(to_v3(cam.lerp_pos),
                                                   Vector3Add(player_pos, {x, y, z}),
                                                   8.0f * dt));
            cam.lerp_target = from_v3(Vector3Lerp(to_v3(cam.lerp_target),
                                                   player_pos,
                                                   12.0f * dt));

            // 5. Compute and store view directions for CharacterInputSystem
            Vector3 fwd = Vector3Normalize(
                Vector3Subtract(to_v3(cam.lerp_target), to_v3(cam.lerp_pos)));
            Vector3 up  = {0, 1, 0};
            Vector3 right = Vector3CrossProduct(fwd, up);

            cam.view_forward = from_v3(fwd);
            cam.view_right   = from_v3(right);
        });
}
