#include "player_input.hpp"
#include "../components.hpp"
#include "../input_state.hpp"
#include <cmath>
#include <algorithm>

using namespace ecs;

void PlayerInputSystem::Update(World& world) {
    auto* input_ptr = world.try_resource<InputRecord>();
    if (!input_ptr) return;
    const auto& record = *input_ptr;

    world.single<PlayerInput>([&](Entity, PlayerInput& input) {
        // Reset per-frame state
        input.move_input = {0, 0};
        input.look_input = {0, 0};
        input.jump = false;
        input.plant_platform = false;
        input.trigger_val = 0.0f;

        // 1. Keyboard Input
        if (record.keys_down[KEY_W]) input.move_input.y += 1.0f;
        if (record.keys_down[KEY_S]) input.move_input.y -= 1.0f;
        if (record.keys_down[KEY_A]) input.move_input.x -= 1.0f;
        if (record.keys_down[KEY_D]) input.move_input.x += 1.0f;
        
        if (record.keys_pressed[KEY_SPACE]) input.jump = true;
        
        if (record.keys_pressed[KEY_E] || record.mouse_buttons_pressed[MOUSE_BUTTON_LEFT]) {
            input.plant_platform = true;
            input.trigger_val = 1.0f;
        }

        // 2. Gamepad Input
        const float deadzone = 0.15f;
        for (const auto& gp : record.gamepads) {
            float lx = gp.axes[GAMEPAD_AXIS_LEFT_X];
            float ly = gp.axes[GAMEPAD_AXIS_LEFT_Y];
            float rx = gp.axes[GAMEPAD_AXIS_RIGHT_X];
            float ry = gp.axes[GAMEPAD_AXIS_RIGHT_Y];

            bool is_active = (std::abs(lx) > deadzone || std::abs(ly) > deadzone || 
                              std::abs(rx) > deadzone || std::abs(ry) > deadzone);

            if (is_active) {
                if (std::abs(lx) > deadzone) input.move_input.x += lx;
                if (std::abs(ly) > deadzone) input.move_input.y += -ly; 
                if (std::abs(rx) > deadzone) input.look_input.x += rx;
                if (std::abs(ry) > deadzone) input.look_input.y += ry;
            }

            if (gp.buttons_pressed[GAMEPAD_BUTTON_RIGHT_FACE_DOWN]) {
                input.jump = true;
            }

            float rt = gp.axes[GAMEPAD_AXIS_RIGHT_TRIGGER];
            float trigger_normalized = (rt + 1.0f) * 0.5f;
            if (trigger_normalized > 0.5f) {
                input.plant_platform = true;
                input.trigger_val = std::max(input.trigger_val, trigger_normalized);
            }
        }

        // 3. Final Input Normalization
        float move_mag_sq = input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y;
        if (move_mag_sq > 1.0f) {
            float mag = std::sqrt(move_mag_sq);
            input.move_input.x /= mag;
            input.move_input.y /= mag;
        }
    });
}
